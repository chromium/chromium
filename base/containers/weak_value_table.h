// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CONTAINERS_WEAK_VALUE_TABLE_H_
#define BASE_CONTAINERS_WEAK_VALUE_TABLE_H_

#include <atomic>
#include <type_traits>

#include "base/check.h"
#include "base/dcheck_is_on.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

// A table that weakly owns thread-safe ref-counted values. The primary
// supported operation is `FindOrCreate()`, which either:
// - returns an owning reference to the value already associated with the key
//   in the table if the key is already present or
// - creates a new value, associates it with key in the table, and returns an
//   owning reference to the new value.
//
// Value type implementations must:
// - inherit from `RefCountedWeakValue<Key, Value>` using the CRTP.
// - provide an idempotent `Getkey()` method that returns the key associated
//   with the given value in the table–this is needed to remove the entry from
//   the table when the refcount reaches zero.
// - be constructed with `base::MakeRefCounted`, since the initial refcount
//   starts at 1.

namespace base::subtle {

template <typename Key, typename ValueT>
class WeakValueTable;

template <typename Key, typename Value>
class RefCountedWeakValue {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  RefCountedWeakValue() = default;
  ~RefCountedWeakValue() = default;

  RefCountedWeakValue(const RefCountedWeakValue&) = delete;
  RefCountedWeakValue& operator=(const RefCountedWeakValue&) = delete;

  [[nodiscard]] bool HasOneRef() const {
    return count_.load(std::memory_order_acquire) == 1;
  }

  void AddRef() {
#if DCHECK_IS_ON()
    DCHECK(!needs_adoption_);
#endif
    CHECK(count_.fetch_add(1, std::memory_order_relaxed) != 0);
  }

  void Release() {
    size_t current = count_.load(std::memory_order_relaxed);
    do {
      // The last refcount needs to be released in coordination with `table_`
      // to avoid races with `AddRef()`.
      if (current == 1) {
        static_cast<Value*>(this)->InternalTestHookForReleaseRace();
        if (table_) {
          // CRTP promises this cast is safe.
          if (table_->ReleaseAndRemoveIfUnused(static_cast<Value*>(this))) {
            // No race to acquire a new reference, so it's safe to delete
            // `this`.
            DeleteThis();
          }
        } else {
          // Not inserted into the table yet, so no coordination needed; just
          // delete `this` directly.
          DeleteThis();
        }
        return;
      }
      // If the compare-and-swap fails, it updates `current` so no need to
      // reload `current`.
    } while (!count_.compare_exchange_weak(current, current - 1,
                                           std::memory_order_acq_rel));
  }

  void Adopted() {
#if DCHECK_IS_ON()
    DCHECK(needs_adoption_);
    needs_adoption_ = false;
#endif
  }

  void InternalTestHookForReleaseRace() {}

 private:
  friend class WeakValueTable<Key, Value>;

  // Returns `true` if the last ref was released.
  [[nodiscard]] bool MaybeReleaseLastRef() {
    return count_.fetch_sub(1, std::memory_order_acq_rel) == 1;
  }

  void DeleteThis() { delete static_cast<Value*>(this); }

  std::atomic<size_t> count_ = 1;
  raw_ptr<WeakValueTable<Key, Value>> table_;
#if DCHECK_IS_ON()
  bool needs_adoption_ = true;
#endif
};

template <typename Key, typename ValueT>
class WeakValueTable {
  static_assert(std::is_base_of_v<RefCountedWeakValue<Key, ValueT>, ValueT>);

 public:
  WeakValueTable() = default;
  ~WeakValueTable() { CHECK(empty()); }

  WeakValueTable(const WeakValueTable&) = delete;
  WeakValueTable& operator=(const WeakValueTable&) = delete;

  // Finds and returns the value keyed to `key` in the table, or invokes
  // `callable` to create a new value to associate with `key` in the table and
  // returns the newly-created value. Creation can race on different threads,
  // since the lock is dropped while invoking `callable`.
  template <typename K, typename Callable>
    requires requires(Callable&& callable) {
      // The constraint is intentionally somewhat strict, since `ValueT*` is
      // also convertible to `scoped_refptr<ValueT>`.
      { callable() } -> std::same_as<scoped_refptr<ValueT>>;
    }
  [[nodiscard]] scoped_refptr<ValueT> FindOrCreate(K&& key,
                                                   Callable&& callable) {
    {
      AutoLock lock(table_lock_);
      auto it = table_.find(key);
      if (it != table_.end()) {
        return scoped_refptr<ValueT>(it->second.get());
      }
    }

    scoped_refptr<ValueT> ptr = callable();
    if (!ptr) {
      return nullptr;
    }

    // Assert this is the sole reference: if multiple threads hold a reference
    // to `ptr`, setting `table_` will dangerously race with `Release()`.
    CHECK(ptr->HasOneRef());

    CHECK(ptr->GetKey() == key);

    AutoLock lock(table_lock_);
    auto [it, inserted] = table_.try_emplace(std::forward<K>(key), ptr.get());
    if (!inserted) {
      // Lost the race; another thread already inserted.
      return WrapRefCounted(it->second.get());
    }
    ptr->table_ = this;
    return ptr;
  }

  // Intended mostly for tests and correctness assertions, as this can be racy.
  [[nodiscard]] bool empty() const {
    AutoLock lock(table_lock_);
    return table_.empty();
  }

 private:
  friend RefCountedWeakValue<Key, ValueT>;

  [[nodiscard]] bool ReleaseAndRemoveIfUnused(ValueT* value) {
    AutoLock lock(table_lock_);
    auto it = table_.find(value->GetKey());
    CHECK(it != table_.end());
    CHECK_EQ(it->second, value);

    if (it->second->MaybeReleaseLastRef()) {
      table_.erase(it);
      return true;
    }

    // Someone acquired a new reference so leave the value in the table.
    return false;
  }

  // Invariant: outside the critical sections guarded by `table_lock_`, every
  // value in the table must have a refcount of at least 1. This invariant is
  // guaranteed by two things:
  // - new values begin with a refcount of exactly 1 when inserted into the
  //   table.
  // - release of the (potentially) final reference to a value is delegated to
  //   `ReleaseAndRemoveIfUnused()`. This reference is only released while
  //   holding `table_lock_`. Either:
  //   - another thread acquired an additional reference via `FindOrCreate()`
  //     before `ReleaseAndRemoveIfUnused()` acquired `table_lock_`: the value
  //     remains in the table.
  //   - the refcount goes to zero and the value is erased from `table_`
  //     before releasing `table_lock_`.
  mutable Lock table_lock_;
  absl::flat_hash_map<Key, raw_ptr<ValueT>> table_ GUARDED_BY(table_lock_);
};

}  // namespace base::subtle

#endif  // BASE_CONTAINERS_WEAK_VALUE_TABLE_H_
