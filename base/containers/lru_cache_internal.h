// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_LRU_CACHE_INTERNAL_H_
#define BASE_CONTAINERS_LRU_CACHE_INTERNAL_H_

#include <stddef.h>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <list>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/memory_coordinator/async_memory_consumer_registration.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_coordinator_features.h"
#include "base/memory_coordinator/traits.h"

namespace base {
namespace trace_event::internal {

template <class LruCacheType>
size_t DoEstimateMemoryUsageForLruCache(const LruCacheType&);

}  // namespace trace_event::internal

namespace internal {

struct GetKeyFromKVPair {
  template <typename T1, typename T2>
  constexpr const T1& operator()(const std::pair<T1, T2>& pair) {
    return pair.first;
  }
};

inline bool IsLRUCacheMemoryConsumerEnabled() {
  return base::FeatureList::GetInstance() &&
         base::FeatureList::IsEnabled(kLRUCacheMemoryConsumer);
}

// Base class for the LRU cache specializations defined below.
template <class ValueType, class GetKeyFromValue, class KeyIndexTemplate>
class LRUCacheBase : public PassiveMemoryConsumer {
 public:
  // The contents of the list. This must contain a copy of the key (that may be
  // extracted via `GetKeyFromValue()(value)` so we can efficiently delete
  // things given an element of the list.
  using value_type = ValueType;

 private:
  using ValueList = std::list<value_type>;
  using KeyIndex =
      typename KeyIndexTemplate::template Type<typename ValueList::iterator>;

 public:
  using size_type = typename ValueList::size_type;
  using key_type = typename KeyIndex::key_type;

  using iterator = typename ValueList::iterator;
  using const_iterator = typename ValueList::const_iterator;
  using reverse_iterator = typename ValueList::reverse_iterator;
  using const_reverse_iterator = typename ValueList::const_reverse_iterator;

  static constexpr std::optional<size_type> NO_AUTO_EVICT = std::nullopt;

  // The max_size is the size at which the cache will prune its members to when
  // a new item is inserted. If the caller wants to manage this itself (for
  // example, maybe it has special work to do when something is evicted), it
  // can pass NO_AUTO_EVICT to not restrict the cache size.
  explicit LRUCacheBase(std::optional<size_type> max_size)
      : baseline_max_size_(max_size) {
    MaybeCreateOrRemoveRegistration();
  }

  // In theory, LRUCacheBase could be copyable, but since copying `ValueList`
  // might be costly, it's currently move-only to ensure users don't
  // accidentally incur performance penalties. If you need this to become
  // copyable, talk to base/ OWNERS.
  LRUCacheBase(LRUCacheBase&& other) noexcept
      : ordering_(std::move(other.ordering_)),
        index_(std::move(other.index_)),
        baseline_max_size_(
            std::exchange(other.baseline_max_size_, std::nullopt)),
        current_memory_limit_(
            other.current_memory_limit_.load(std::memory_order_relaxed)) {
    other.registration_.reset();
    MaybeCreateOrRemoveRegistration();
  }

  LRUCacheBase& operator=(LRUCacheBase&& other) noexcept {
    if (this != &other) {
      ordering_ = std::move(other.ordering_);
      index_ = std::move(other.index_);
      baseline_max_size_ =
          std::exchange(other.baseline_max_size_, std::nullopt);
      current_memory_limit_.store(
          other.current_memory_limit_.load(std::memory_order_relaxed),
          std::memory_order_relaxed);
      other.registration_.reset();
      registration_.reset();
      MaybeCreateOrRemoveRegistration();
    }
    return *this;
  }

  ~LRUCacheBase() override = default;

  // Returns the maximum size of the cache. Valid to call only if there is a max
  // size (NO_AUTO_EVICT was *not* used).
  size_type max_size() const {
    CHECK(baseline_max_size_.has_value());

    if (!registration_ || baseline_max_size_.value() == 0) {
      return baseline_max_size_.value();
    }

    // Some callers (mostly tests) set a max size of 0. We can't support 0
    // out-of-the box for all callers, because some of them depend on insertions
    // being guaranteed to work. Because of this, the max size imposed by
    // MemoryCoordinator is >= 1, but we still need to honor a caller-passed max
    // size of 0.
    return std::max<size_type>(
        1, ScaleByMemoryLimit(
               baseline_max_size_.value(),
               current_memory_limit_.load(std::memory_order_relaxed)));
  }

  void UpdateMaxSize(size_type new_max_size) {
    baseline_max_size_ = new_max_size;
    MaybeCreateOrRemoveRegistration();
    ShrinkToSize(max_size());
  }

  // Inserts an item into the list. If an existing item has the same key, it is
  // removed prior to insertion. An iterator indicating the inserted item will
  // be returned (this will always be the front of the list).
  // In the map variations of this container, `value_type` is a `std::pair` and
  // it's preferred to use the `Put(k, v)` overload of this method.
  iterator Put(value_type&& value) {
    // Remove any existing item with that key.
    key_type key = GetKeyFromValue{}(value);
    typename KeyIndex::iterator index_iter = index_.find(key);
    if (index_iter != index_.end()) {
      // Erase the reference to it. The index reference will be replaced in the
      // code below.
      Erase(index_iter->second);
    }

    if (baseline_max_size_.has_value()) {
      // The only way an insertion can fail is if max size is zero. In other
      // cases, another item will be evicted to make room for the new item.
      if (max_size() == 0u) {
        return end();
      }

      // New item is being inserted which might make it larger than the maximum
      // size: kick the oldest thing out if necessary.
      ShrinkToSize(max_size() - 1);
    }

    ordering_.push_front(std::move(value));
    index_.emplace(std::move(key), ordering_.begin());
    return ordering_.begin();
  }

  // Inserts an item into the list. If an existing item has the same key, it is
  // removed prior to insertion. An iterator indicating the inserted item will
  // be returned (this will always be the front of the list).
  template <class K, class V>
    requires(std::same_as<GetKeyFromValue, GetKeyFromKVPair>)
  iterator Put(K&& key, V&& value) {
    return Put(value_type{std::forward<K>(key), std::forward<V>(value)});
  }

  // Retrieves the contents of the given key, or end() if not found. This method
  // has the side effect of moving the requested item to the front of the
  // recency list.
  template <typename K = key_type>
    requires requires(KeyIndex index, K key) { index.find(key); }
  iterator Get(const K& key) {
    typename KeyIndex::iterator index_iter = index_.find(key);
    if (index_iter == index_.end()) {
      return end();
    }
    typename ValueList::iterator iter = index_iter->second;

    // Move the touched item to the front of the recency ordering.
    ordering_.splice(ordering_.begin(), ordering_, iter);
    return ordering_.begin();
  }

  // Retrieves the item associated with a given key and returns it via
  // result without affecting the ordering (unlike Get()).
  template <typename K = key_type>
    requires requires(KeyIndex index, K key) { index.find(key); }
  iterator Peek(const K& key) {
    typename KeyIndex::const_iterator index_iter = index_.find(key);
    if (index_iter == index_.end()) {
      return end();
    }
    return index_iter->second;
  }

  template <typename K = key_type>
    requires requires(KeyIndex index, K key) { index.find(key); }
  const_iterator Peek(const K& key) const {
    typename KeyIndex::const_iterator index_iter = index_.find(key);
    if (index_iter == index_.end()) {
      return end();
    }
    return index_iter->second;
  }

  // Exchanges the contents of |this| by the contents of the |other|.
  void Swap(LRUCacheBase& other) {
    ordering_.swap(other.ordering_);
    index_.swap(other.index_);
    std::swap(baseline_max_size_, other.baseline_max_size_);
    int this_limit = current_memory_limit_.load(std::memory_order_relaxed);
    int other_limit =
        other.current_memory_limit_.load(std::memory_order_relaxed);
    current_memory_limit_.store(other_limit, std::memory_order_relaxed);
    other.current_memory_limit_.store(this_limit, std::memory_order_relaxed);
    MaybeCreateOrRemoveRegistration();
    other.MaybeCreateOrRemoveRegistration();
  }

  // Erases the item referenced by the given iterator. An iterator to the item
  // following it will be returned. The iterator must be valid.
  // Note that caller should avoid using std::remove_if() with this container as
  // the iterator from begin()/end() is not designed to have the key modified,
  // see comment on begin().
  iterator Erase(iterator pos) {
    index_.erase(GetKeyFromValue()(*pos));
    return ordering_.erase(pos);
  }

  // LRUCache entries are often processed in reverse order, so we add this
  // convenience function (not typically defined by STL containers).
  reverse_iterator Erase(reverse_iterator pos) {
    // We have to actually give it the incremented iterator to delete, since
    // the forward iterator that base() returns is actually one past the item
    // being iterated over.
    return reverse_iterator(Erase((++pos).base()));
  }

  // Shrinks the cache so it only holds |new_size| items. If |new_size| is
  // bigger or equal to the current number of items, this will do nothing.
  void ShrinkToSize(size_type new_size) {
    for (size_type i = size(); i > new_size; i--) {
      Erase(rbegin());
    }
  }

  // Deletes everything from the cache.
  void Clear() {
    index_.clear();
    ordering_.clear();
  }

  // Returns the number of elements in the cache.
  size_type size() const {
    // We don't use ordering_.size() for the return value because
    // (as a linked list) it can be O(n).
    DCHECK(index_.size() == ordering_.size());
    return index_.size();
  }

  // Allows iteration over the list. Forward iteration starts with the most
  // recent item and works backwards.
  //
  // Note that since these iterators are actually iterators over a list, you
  // can keep them as you insert or delete things (as long as you don't delete
  // the one you are pointing to) and they will still be valid.
  // Also, caller should avoid moving the order of items around, or any
  // operation that modifies the key in the value with these iterators, such as
  // using std::remove_if(). This is because the key in index_ is not updated
  // and the container will be corrupted.
  iterator begin() { return ordering_.begin(); }
  const_iterator begin() const { return ordering_.begin(); }
  iterator end() { return ordering_.end(); }
  const_iterator end() const { return ordering_.end(); }

  reverse_iterator rbegin() { return ordering_.rbegin(); }
  const_reverse_iterator rbegin() const { return ordering_.rbegin(); }
  reverse_iterator rend() { return ordering_.rend(); }
  const_reverse_iterator rend() const { return ordering_.rend(); }

  struct IndexRange {
    using iterator = KeyIndex::const_iterator;

    IndexRange(const iterator& begin, const iterator& end)
        : begin_(begin), end_(end) {}

    iterator begin() const { return begin_; }
    iterator end() const { return end_; }

   private:
    iterator begin_;
    iterator end_;
  };
  // Allows iterating the index, which can be useful when the index is ordered.
  IndexRange index() const { return IndexRange(index_.begin(), index_.end()); }

  bool empty() const { return ordering_.empty(); }

 protected:
  // PassiveMemoryConsumer:
  void OnUpdateMemoryLimit() override {
    current_memory_limit_.store(memory_limit(), std::memory_order_relaxed);
  }

 private:
  template <class LruCacheType>
  friend size_t trace_event::internal::DoEstimateMemoryUsageForLruCache(
      const LruCacheType&);

  static constexpr MemoryConsumerTraits kDefaultTraits =
      MemoryConsumerTraits(MemoryConsumerTraits::ConsumerType::kPassive);

  void MaybeCreateOrRemoveRegistration() {
    if (IsLRUCacheMemoryConsumerEnabled()) {
      if (baseline_max_size_.has_value() && !registration_) {
        registration_.emplace("LRUCache", kDefaultTraits, this);
      }

      if (!baseline_max_size_.has_value() && registration_) {
        registration_.reset();
      }
    }
  }

  ValueList ordering_;
  KeyIndex index_;

  // Indicates the requested maximum size of the cache. If nullopt, there is no
  // maximum size and eviction is not done automatically.
  std::optional<size_type> baseline_max_size_;

  // Caches the current memory limit reported by the MemoryCoordinator.
  std::atomic<int> current_memory_limit_{MemoryConsumer::kDefaultMemoryLimit};

  std::optional<AsyncMemoryConsumerRegistration> registration_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_CONTAINERS_LRU_CACHE_INTERNAL_H_
