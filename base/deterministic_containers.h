// Copyright (c) 2021 Record Replay Inc.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DETERMINISTIC_CONTAINERS_H_
#define BASE_DETERMINISTIC_CONTAINERS_H_

// When recording/replaying, sometimes containers must be iterated in the same
// order when replaying as they did when recording in order for behavior to
// remain the same. STL unordered containers (aka hash tables) do not ensure
// this. This file defines replacements for these containers that have the
// same interface but always iterate their contents in insertion order.

namespace base {

template <typename Key,
          typename T,
          typename Hash = std::hash<Key>,
          typename EqualTo = std::equal_to<Key>>
class deterministic_unordered_map {
  // All entries in the map in insertion order. Entries which have been erased
  // are empty. If the map has many erasures over time then this vector will
  // grow without bound. It would be nice to occasionally clean out these old
  // entries.
  typedef std::vector<base::Optional<std::pair<Key, T>>> InnerVector;
  InnerVector vector_;

  // Map all keys in the map to indexes in vector_.
  typedef std::unordered_map<Key, size_t, Hash, EqualTo> InnerMap;
  InnerMap map_;

 public:
  struct iterator : std::iterator<std::forward_iterator_tag,
                                  std::pair<Key, T>,
                                  long,
                                  std::pair<Key, T>*,
                                  std::pair<Key, T>&> {
    size_t index_;
    InnerVector& vector_;
    iterator(size_t index, InnerVector& vector)
      : index_(index), vector_(vector) {}

    iterator& operator++() {
      index_++;
      while (index_ < vector_.size() && !vector_[index_].has_value()) {
        index_++;
      }
      return *this;
    }

    iterator& operator++(int) {
      iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator !=(const iterator& o) const {
      CHECK(&vector_ == &o.vector_);
      return index_ != o.index_;
    }

    bool operator ==(const iterator& o) const {
      CHECK(&vector_ == &o.vector_);
      return index_ == o.index_;
    }

    std::pair<Key, T>& operator *() const {
      return vector_[index_].value();
    }

    std::pair<Key, T>* operator ->() const {
      return &vector_[index_].value();
    }
  };

  struct const_iterator : std::iterator<std::forward_iterator_tag,
                                        std::pair<Key, T>,
                                        long,
                                        const std::pair<Key, T>*,
                                        const std::pair<Key, T>&> {
    size_t index_;
    const InnerVector& vector_;
    const_iterator(size_t index, const InnerVector& vector)
      : index_(index), vector_(vector) {}

    const_iterator& operator++() {
      index_++;
      while (index_ < vector_.size() && !vector_[index_].has_value()) {
        index_++;
      }
      return *this;
    }

    const_iterator& operator++(int) {
      const_iterator tmp = *this;
      ++(*this);
      return tmp;
    }

    bool operator !=(const const_iterator& o) const {
      CHECK(&vector_ == &o.vector_);
      return index_ != o.index_;
    }

    bool operator ==(const const_iterator& o) const {
      CHECK(&vector_ == &o.vector_);
      return index_ == o.index_;
    }

    const std::pair<Key, T>& operator *() const {
      return vector_[index_].value();
    }

    const std::pair<Key, T>* operator ->() const {
      return &vector_[index_].value();
    }
  };

  iterator find(const Key& k) {
    auto iter = map_.find(k);
    return iter != map_.end() ? iterator(iter->second, vector_) : end();
  }

  const_iterator find(const Key& k) const {
    auto iter = map_.find(k);
    return iter != map_.end() ? const_iterator(iter->second, vector_) : end();
  }

  iterator begin() {
    for (size_t i = 0; i < vector_.size(); i++) {
      if (vector_[i].has_value()) {
        return iterator(i, vector_);
      }
    }
    return end();
  }

  const_iterator begin() const {
    for (size_t i = 0; i < vector_.size(); i++) {
      if (vector_[i].has_value()) {
        return const_iterator(i, vector_);
      }
    }
    return end();
  }

  iterator end() { return iterator(vector_.size(), vector_); }
  const_iterator end() const { return const_iterator(vector_.size(), vector_); }

  T& operator[](const Key& k) {
    auto iter = find(k);
    return (iter != end()) ? iter->second : insert(k, T()).first->second;
  }

  std::pair<iterator, bool> insert(const Key& k, const T& v) {
    auto iter = find(k);
    if (iter != end()) {
      return { iter, false };
    }
    size_t index = vector_.size();
    vector_.emplace_back();
    vector_.back().emplace(k, v);
    map_[k] = index;
    return { iterator(vector_.size() - 1, vector_), true };
  }

  size_t erase(const Key& k) {
    auto iter = map_.find(k);
    if (iter == map_.end()) {
      return 0;
    }
    CHECK(iter->second < vector_.size() && vector_[iter->second].has_value());
    vector_[iter->second].reset();
    map_.erase(iter);
    return 1;
  }

  size_t size() const {
    return map_.size();
  }
};

} // namespace base

#endif // BASE_DETERMINISTIC_CONTAINERS_H_
