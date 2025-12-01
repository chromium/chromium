// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_address_hash_set.h"

#include <stdlib.h>

#include <atomic>
#include <cinttypes>
#include <memory>

#include "base/debug/alias.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/lock.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/simple_thread.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using ContainsResult = LockFreeAddressHashSet::ContainsResult;

class LockFreeAddressHashSetTest : public ::testing::TestWithParam<bool> {
 public:
  LockFreeAddressHashSetTest() {
    scoped_feature_list_.InitWithFeatureState(kUseLockFreeBloomFilter,
                                              GetParam());
  }

  static bool IsSubset(const LockFreeAddressHashSet& superset,
                       const LockFreeAddressHashSet& subset) {
    for (const LockFreeAddressHashSet::Bucket& bucket : subset.buckets_) {
      for (const LockFreeAddressHashSet::Node* node =
               bucket.head.load(std::memory_order_acquire);
           node; node = node->next) {
        void* key = node->key.load(std::memory_order_relaxed);
        if (key && superset.Contains(key) != ContainsResult::kFound) {
          return false;
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
             set.buckets_[bucket].head.load(std::memory_order_acquire);
         node; node = node->next) {
      if (node->key.load(std::memory_order_relaxed) != nullptr) {
        ++count;
      }
    }
    return count;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

using LockFreeAddressHashSetDeathTest = LockFreeAddressHashSetTest;

INSTANTIATE_TEST_SUITE_P(EnableBloomFilter,
                         LockFreeAddressHashSetTest,
                         ::testing::Bool());
INSTANTIATE_TEST_SUITE_P(EnableBloomFilter,
                         LockFreeAddressHashSetDeathTest,
                         ::testing::Bool());

TEST_P(LockFreeAddressHashSetTest, EmptySet) {
  Lock lock;
  LockFreeAddressHashSet set(8, lock);

  AutoLock auto_lock(lock);
  EXPECT_EQ(size_t(0), set.size());
  EXPECT_EQ(size_t(8), set.buckets_count());
  EXPECT_EQ(0., set.load_factor());
  EXPECT_NE(set.Contains(&set), ContainsResult::kFound);
}

TEST_P(LockFreeAddressHashSetTest, BasicOperations) {
  Lock lock;
  LockFreeAddressHashSet set(8, lock);

  AutoLock auto_lock(lock);
  for (size_t i = 1; i <= 100; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Insert(ptr);
    EXPECT_EQ(i, set.size());
    EXPECT_EQ(set.Contains(ptr), ContainsResult::kFound);
  }

  size_t size = 100;
  EXPECT_EQ(size, set.size());
  EXPECT_EQ(size_t(8), set.buckets_count());
  EXPECT_EQ(size / 8., set.load_factor());

  for (size_t i = 99; i >= 3; i -= 3) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Remove(ptr);
    EXPECT_EQ(--size, set.size());
    EXPECT_NE(set.Contains(ptr), ContainsResult::kFound);
  }
  // Removed every 3rd value (33 total) from the set, 67 have left.
  EXPECT_EQ(set.size(), 67u);

  for (size_t i = 1; i <= 100; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    EXPECT_EQ(i % 3 != 0, set.Contains(ptr) == ContainsResult::kFound);
  }
}

TEST_P(LockFreeAddressHashSetTest, Copy) {
  Lock lock;
  LockFreeAddressHashSet set(16, lock);

  AutoLock auto_lock(lock);
  for (size_t i = 1000; i <= 16000; i += 1000) {
    void* ptr = reinterpret_cast<void*>(i);
    set.Insert(ptr);
  }

  LockFreeAddressHashSet set2(4, lock);
  LockFreeAddressHashSet set3(64, lock);
  set2.Copy(set);
  set3.Copy(set);

  EXPECT_TRUE(Equals(set, set2));
  EXPECT_TRUE(Equals(set, set3));
  EXPECT_TRUE(Equals(set2, set3));

  set.Insert(reinterpret_cast<void*>(42));

  EXPECT_FALSE(Equals(set, set2));
  EXPECT_FALSE(Equals(set, set3));
  EXPECT_TRUE(Equals(set2, set3));

  EXPECT_TRUE(IsSubset(set, set2));
  EXPECT_FALSE(IsSubset(set2, set));
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
      EXPECT_EQ(set_->Contains(ptr), ContainsResult::kFound);
      {
        AutoLock auto_lock(*lock_);
        set_->Remove(ptr);
      }
      EXPECT_NE(set_->Contains(ptr), ContainsResult::kFound);
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
  LockFreeAddressHashSet set(16, lock);

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
      EXPECT_EQ(i < 16, set.Contains(reinterpret_cast<void*>(i)) ==
                            ContainsResult::kFound);
    }
  }
  cancel.store(true, std::memory_order_release);
  thread->Join();

  EXPECT_EQ(set.Contains(reinterpret_cast<void*>(0x1337)),
            ContainsResult::kFound);
  EXPECT_NE(set.Contains(reinterpret_cast<void*>(0xbadf00d)),
            ContainsResult::kFound);
}

TEST_P(LockFreeAddressHashSetTest, BucketsUsage) {
  // Test the uniformity of buckets usage.
  size_t count = 10000;
  Lock lock;
  LockFreeAddressHashSet set(16, lock);
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
  LockFreeAddressHashSet set(8, lock);
  LockFreeAddressHashSet set2(8, lock);

  // Should not require lock.
  EXPECT_NE(set.Contains(&lock), ContainsResult::kFound);
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
