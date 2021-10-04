#include "SimpleLRU.h"

namespace Afina {
namespace Backend {

void SimpleLRU::remove_head() {
    _size -= _lru_head->key.size() + _lru_head->value.size();
    _lru_index.erase(_lru_head->key);
    if (_lru_head.get() == _lru_tail)
        _lru_head.reset(nullptr);
    else {
        _lru_head = std::move(_lru_head->next);
        _lru_head->prev = nullptr;
    }
}

void SimpleLRU::remove_tail() {
    _size -= _lru_tail->key.size() + _lru_tail->value.size();
    _lru_index.erase(_lru_tail->key);
    if (_lru_head.get() == _lru_tail)
        _lru_head.reset(nullptr);
    else {
        _lru_tail = _lru_tail->prev;
        _lru_tail->next.reset(nullptr);
    }
}

void SimpleLRU::add_to_tail(const std::string &key, const std::string &value) {
    while (key.size() + value.size() + _size > _max_size)
        remove_head();

    auto new_node = new lru_node{key, value, _lru_tail, nullptr};

    if (!_lru_head.get())
        _lru_head.reset(new_node);

    _lru_tail = new_node;
    if (_lru_tail->prev)
        (_lru_tail->prev->next).reset(_lru_tail);

    _lru_index.insert(
        {std::reference_wrapper<const std::string>(new_node->key), std::reference_wrapper<lru_node>(*new_node)});
    _size += key.size() + value.size();
}

void SimpleLRU::move_to_tail(lru_node &node) {
    lru_node *node_ptr = &node;
    if (node_ptr == _lru_tail)
        return;

    if (node_ptr->prev) {
        (node_ptr->next).get()->prev = node_ptr->prev;
        node_ptr->prev->next.release();
        node_ptr->prev->next = std::move(node_ptr->next);

        node_ptr->next.release();
        node_ptr->prev = _lru_tail;
        _lru_tail = node_ptr;
        (_lru_tail->prev->next).reset(_lru_tail);
    } else {
        lru_node *temp = _lru_head->next.get();
        _lru_head->next.release();
        node_ptr->prev = _lru_tail;
        node_ptr->prev->next.reset(node_ptr);
        _lru_tail = node_ptr;
        _lru_head.release();
        _lru_head.reset(temp);
        _lru_head->prev = nullptr;
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size)
        return false;

    auto node = _lru_index.find(key);
    if (node == _lru_index.end())
        add_to_tail(key, value);
    else
        Set(key, value);

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    if ((_lru_index.find(key) != _lru_index.end()) || key.size() + value.size() > _max_size)
        return false;
    add_to_tail(key, value);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    auto node = _lru_index.find(key);
    if (node == _lru_index.end())
        return false;

    if (value.size() > _max_size)
        return false;

    move_to_tail((node->second).get());
    while (value.size() - _lru_tail->value.size() + _size > _max_size)
        remove_head();

    _size += value.size() - _lru_tail->value.size();
    _lru_tail->value = value;

    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto node = _lru_index.find(key);
    if (node == _lru_index.end())
        return false;

    move_to_tail((node->second).get());
    remove_tail();
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto node = _lru_index.find(key);
    if (node == _lru_index.end())
        return false;
    move_to_tail((node->second).get());
    value = _lru_tail->value;
    return true;
}

} // namespace Backend
} // namespace Afina
