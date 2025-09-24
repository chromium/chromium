// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"

#include <stdlib.h>

#include <algorithm>
#include <atomic>
#include <cinttypes>
#include <memory>

#include "base/debug/alias.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/threading/simple_thread.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Param is `multi_key`.
class LockFreeAddressHashSetTest : public ::testing::TestWithParam<bool> {
 public:
  // Returns the number of keys per node used in this test.
  size_t GetKeysPerNode() {
    return GetParam() ? LockFreeAddressHashSet::kKeysPerNode : 1;
  }

  static bool IsSubset(const LockFreeAddressHashSet& superset,
                       const LockFreeAddressHashSet& subset) {
    for (const std::atomic<LockFreeAddressHashSet::Node*>& bucket :
         subset.buckets_) {
      for (const LockFreeAddressHashSet::Node* node =
               bucket.load(std::memory_order_acquire);
           node; node = node->next) {
        for (const LockFreeAddressHashSet::KeySlot& key_slot :
             subset.GetKeySlots(node)) {
          void* key = key_slot.load(std::memory_order_relaxed);
          if (key != nullptr && key != LockFreeAddressHashSet::kDeletedKey &&
              !superset.Contains(key)) {
            return false;
          }
        }
      }
    }
    return true;
  }

  static bool Equals(const LockFreeAddressHashSet& set1,
                     const LockFreeAddressHashSet& set2) {
    return IsSubset(set1, set2) && IsSubset(set2, set1);
  }

  // Returns the number of keys in `bucket`.
  static size_t BucketSize(const LockFreeAddressHashSet& set, size_t bucket) {
    size_t count = 0;
    for (const LockFreeAddressHashSet::Node* node =
             set.buckets_[bucket].load(std::memory_order_acquire);
         node; node = node->next) {
      for (const LockFreeAddressHashSet::KeySlot& key_slot :
           set.GetKeySlots(node)) {
        void* key = key_slot.load(std::memory_order_relaxed);
        if (key != nullptr && key != LockFreeAddressHashSet::kDeletedKey) {
          ++count;
        }
      }
    }
    return count;
  }

  // Returns the number of available slots in `bucket`, whether or not they
  // contain keys.
  static size_t BucketCapacity(const LockFreeAddressHashSet& set,
                               size_t bucket) {
    size_t capacity = 0;
    for (const LockFreeAddressHashSet::Node* node =
             set.buckets_[bucket].load(std::memory_order_acquire);
         node; node = node->next) {
      capacity += set.GetKeySlots(node).size();
    }
    return capacity;
  }

  static ::testing::AssertionResult ValidateNullAndDeletedKeys(
      const LockFreeAddressHashSet& set,
      size_t expected_deleted_keys) {
    size_t deleted_keys = 0;
    for (const std::atomic<LockFreeAddressHashSet::Node*>& bucket :
         set.buckets_) {
      for (const LockFreeAddressHashSet::Node* node =
               bucket.load(std::memory_order_acquire);
           node; node = node->next) {
        bool found_null_key = false;
        for (const LockFreeAddressHashSet::KeySlot& key_slot :
             set.GetKeySlots(node)) {
          void* key = key_slot.load(std::memory_order_relaxed);
          if (found_null_key && key != nullptr) {
            return ::testing::AssertionFailure()
                   << "null keys must be at end of list";
          }
          if (key == nullptr) {
            found_null_key = true;
          } else if (key == LockFreeAddressHashSet::kDeletedKey) {
            ++deleted_keys;
          }
        }
      }
    }
    if (deleted_keys != expected_deleted_keys) {
      return ::testing::AssertionFailure()
             << "found " << deleted_keys << " deleted keys, expected "
             << expected_deleted_keys;
    }
    return ::testing::AssertionSuccess();
  }
};

using LockFreeAddressHashSetDeathTest = LockFreeAddressHashSetTest;

INSTANTIATE_TEST_SUITE_P(All, LockFreeAddressHashSetTest, ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         LockFreeAddressHashSetDeathTest,
                         ::testing::Bool());

TEST_P(LockFreeAddressHashSetTest, EmptySet) {
  Lock lock;
  LockFreeAddressHashSet set(8, lock, GetParam());

  AutoLock auto_lock(lock);
  EXPECT_EQ(size_t(0), set.size());
  EXPECT_EQ(size_t(8), set.buckets_count());
  EXPECT_EQ(0., set.load_factor());
  EXPECT_FALSE(set.Contains(&set));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 0));
}

TEST_P(LockFreeAddressHashSetTest, BasicOperations) {
  Lock lock;
  LockFreeAddressHashSet set(8, lock, GetParam());

  AutoLock auto_lock(lock);
  for (size_t i = 1; i <= 100; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Insert(ptr);
    EXPECT_EQ(i, set.size());
    EXPECT_TRUE(set.Contains(ptr));
    EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 0));
  }

  size_t size = 100;
  EXPECT_EQ(size, set.size());
  EXPECT_EQ(size_t(8), set.buckets_count());
  EXPECT_EQ(size / 8., set.load_factor());

  size_t deleted_keys = 0;
  for (size_t i = 99; i >= 3; i -= 3) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Remove(ptr);
    EXPECT_EQ(--size, set.size());
    EXPECT_FALSE(set.Contains(ptr));
    EXPECT_TRUE(ValidateNullAndDeletedKeys(set, ++deleted_keys));
  }
  // Removed every 3rd value (33 total) from the set, 67 have left.
  EXPECT_EQ(deleted_keys, 33u);
  EXPECT_EQ(set.size(), 67u);

  for (size_t i = 1; i <= 100; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    EXPECT_EQ(i % 3 != 0, set.Contains(ptr));
  }
}

TEST_P(LockFreeAddressHashSetTest, Copy) {
  Lock lock;
  LockFreeAddressHashSet set(16, lock, GetParam());

  AutoLock auto_lock(lock);
  for (size_t i = 1000; i <= 16000; i += 1000) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Insert(ptr);
  }
  // Remove a key from the set. Copying should not include the kDeletedKey
  // sentinel.
  set.Remove(reinterpret_cast<void*>(2000));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 1));

  LockFreeAddressHashSet set2(4, lock, GetParam());
  LockFreeAddressHashSet set3(64, lock, GetParam());
  set2.Copy(set);
  set3.Copy(set);

  EXPECT_TRUE(Equals(set, set2));
  EXPECT_TRUE(Equals(set, set3));
  EXPECT_TRUE(Equals(set2, set3));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set2, 0));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set3, 0));

  set.Insert(reinterpret_cast<void*>(42));

  EXPECT_FALSE(Equals(set, set2));
  EXPECT_FALSE(Equals(set, set3));
  EXPECT_TRUE(Equals(set2, set3));

  EXPECT_TRUE(IsSubset(set, set2));
  EXPECT_FALSE(IsSubset(set2, set));
}

TEST_P(LockFreeAddressHashSetTest, DeletedSlotIsReused) {
  Lock lock;

  // Put all keys in one bucket.
  LockFreeAddressHashSet set(1, lock, GetParam());

  // Start with at least 3 keys, filling at least one node.
  const size_t initial_keys = std::max(size_t{3}, GetKeysPerNode());

  AutoLock auto_lock(lock);
  for (uintptr_t i = 1; i <= initial_keys; ++i) {
    set.Insert(reinterpret_cast<void*>(i));
    EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 0));
    EXPECT_EQ(BucketSize(set, 0), i);
  }
  size_t capacity = BucketCapacity(set, 0);

  // Keys will have been added in order. Delete keys at the beginning, middle
  // and end of the list.
  set.Remove(reinterpret_cast<void*>(1));
  set.Remove(reinterpret_cast<void*>(2));
  set.Remove(reinterpret_cast<void*>(initial_keys));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 3));
  EXPECT_EQ(BucketSize(set, 0), initial_keys - 3);
  EXPECT_EQ(BucketCapacity(set, 0), capacity);

  // Add more keys. The first 3 should reuse the deleted slots.
  for (uintptr_t i = 1; i <= 3; ++i) {
    void* ptr = reinterpret_cast<void*>(initial_keys + i);
    set.Insert(ptr);
    EXPECT_TRUE(set.Contains(ptr));
    EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 3 - i));
    EXPECT_EQ(BucketSize(set, 0), initial_keys - 3 + i);
    EXPECT_EQ(BucketCapacity(set, 0), capacity);
  }

  // Out of deleted slots so adding another key should grow the bucket.
  void* ptr = reinterpret_cast<void*>(initial_keys + 4);
  set.Insert(ptr);
  EXPECT_TRUE(set.Contains(ptr));
  EXPECT_TRUE(ValidateNullAndDeletedKeys(set, 0));
  EXPECT_EQ(BucketSize(set, 0), initial_keys + 1);
  EXPECT_EQ(BucketCapacity(set, 0), capacity + GetKeysPerNode());
}

class WriterThread : public SimpleThread {
 public:
  WriterThread(LockFreeAddressHashSet& set,
               Lock& lock,
               std::atomic_bool& cancel)
      : SimpleThread("ReaderThread"), set_(set), lock_(lock), cancel_(cancel) {}

  void Run() override {
    for (size_t value = 42; !cancel_->load(std::memory_order_acquire);
         ++value) {
      void* ptr = reinterpret_cast<void*>(value);
      {
        AutoLock auto_lock(*lock_);
        set_->Insert(ptr);
      }
      EXPECT_TRUE(set_->Contains(ptr));
      {
        AutoLock auto_lock(*lock_);
        set_->Remove(ptr);
      }
      EXPECT_FALSE(set_->Contains(ptr));
    }
    // Leave a key for reader to test.
    AutoLock auto_lock(*lock_);
    set_->Insert(reinterpret_cast<void*>(0x1337));
  }

 private:
  raw_ref<LockFreeAddressHashSet> set_;
  raw_ref<Lock> lock_;
  raw_ref<std::atomic_bool> cancel_;
};

TEST_P(LockFreeAddressHashSetTest, ConcurrentAccess) {
  // The purpose of this test is to make sure adding/removing keys concurrently
  // does not disrupt the state of other keys.
  Lock lock;
  LockFreeAddressHashSet set(16, lock, GetParam());

  {
    AutoLock auto_lock(lock);
    for (size_t i = 1; i <= 20; ++i) {
      set.Insert(reinterpret_cast<void*>(i));
    }
    // Remove some items to test empty nodes.
    for (size_t i = 16; i <= 20; ++i) {
      set.Remove(reinterpret_cast<void*>(i));
    }
  }

  std::atomic_bool cancel(false);
  auto thread = std::make_unique<WriterThread>(set, lock, cancel);
  thread->Start();

  for (size_t k = 0; k < 100000; ++k) {
    for (size_t i = 1; i <= 30; ++i) {
      EXPECT_EQ(i < 16, set.Contains(reinterpret_cast<void*>(i)));
    }
  }
  cancel.store(true, std::memory_order_release);
  thread->Join();

  EXPECT_TRUE(set.Contains(reinterpret_cast<void*>(0x1337)));
  EXPECT_FALSE(set.Contains(reinterpret_cast<void*>(0xbadf00d)));
}

TEST_P(LockFreeAddressHashSetTest, BucketsUsage) {
  // Test the uniformity of buckets usage.
  size_t count = 10000;
  Lock lock;
  LockFreeAddressHashSet set(16, lock, GetParam());
  AutoLock auto_lock(lock);
  EXPECT_EQ(set.GetBucketStats().chi_squared, 1.00);
  for (size_t i = 0; i < count; ++i) {
    set.Insert(reinterpret_cast<void*>(0x10000 + 0x10 * i));
  }
  size_t average_per_bucket = count / set.buckets_count();
  for (size_t i = 0; i < set.buckets_count(); ++i) {
    size_t usage = BucketSize(set, i);
    EXPECT_LT(average_per_bucket * 95 / 100, usage);
    EXPECT_GT(average_per_bucket * 105 / 100, usage);
  }
  // A good hash function should always yield chi-squared values between 0.95
  // and 1.05. If this fails, update LockFreeAddressHashSet::Hash. (See
  // https://en.wikipedia.org/wiki/Hash_function#Testing_and_measurement.)
  EXPECT_GE(set.GetBucketStats().chi_squared, 0.95);
  EXPECT_LE(set.GetBucketStats().chi_squared, 1.05);
}

TEST_P(LockFreeAddressHashSetDeathTest, LockAsserts) {
  Lock lock;
  LockFreeAddressHashSet set(8, lock, GetParam());
  LockFreeAddressHashSet set2(8, lock, GetParam());

  // Should not require lock.
  EXPECT_FALSE(set.Contains(&lock));
  EXPECT_EQ(set.buckets_count(), 8);

  // Should require lock.
  {
    AutoLock auto_lock(lock);
    set.Insert(&lock);
    set.Remove(&lock);
    set.Copy(set2);
    EXPECT_EQ(set.size(), 0u);
    EXPECT_EQ(set.load_factor(), 0.0);
    EXPECT_EQ(set.GetBucketStats().lengths.size(), 8u);
  }
  EXPECT_DCHECK_DEATH(set.Insert(&lock));
  EXPECT_DCHECK_DEATH(set.Remove(&lock));
  EXPECT_DCHECK_DEATH(set.Copy(set2));
  EXPECT_DCHECK_DEATH(set.size());
  EXPECT_DCHECK_DEATH(set.load_factor());
  EXPECT_DCHECK_DEATH(set.GetBucketStats());
}

}  // namespace base
