// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"

#include <math.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/synchronization/atomic_flag.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {

namespace {

// With fake hash function N = (value >> N) % 64, these hash to the following
// (note the deliberate hash collisions):
//
// | Pointer  | N=0 | N=1 | N=2 |
// |--------- +-----+-----+-----|
// | kAlfa    |  16 |   8 |   4 |
// | kBravo   |  17 |   8 |   4 |
// | kCharlie |  18 |   9 |   4 |
// | kDelta   |  32 |  16 |   8 |
// | kEcho    |  16 |  40 |  20 |
constexpr uintptr_t kAlfa = 0x10;
constexpr uintptr_t kBravo = 0x11;
constexpr uintptr_t kCharlie = 0x12;
constexpr uintptr_t kDelta = 0x20;
constexpr uintptr_t kEcho = 0x50;

// Writer thread spawned by the ConcurrentAccess test to write a value to
// multiple filters. Multiple writers will race to write different values to
// the same set of filters.
class WriterThread : public SimpleThread {
 public:
  using LockFreeBloomFilterType = LockFreeBloomFilter<2>;

  WriterThread(uintptr_t start_value,
               uintptr_t max_value,
               LockFreeBloomFilterType& filter,
               base::TestWaitableEvent& all_started_event,
               base::AtomicFlag& cancel_flag,
               base::RepeatingClosure on_started_closure)
      : SimpleThread("WriterThread"),
        start_value_(start_value),
        max_value_(max_value),
        filter_(filter),
        all_started_event_(all_started_event),
        cancel_flag_(cancel_flag),
        on_started_closure_(on_started_closure) {}

  void Run() override {
    // Notify that this thread is running, and wait for all the other threads to
    // start.
    std::move(on_started_closure_).Run();
    all_started_event_->Wait();

    // Repeatedly write to the filter starting at `value`, leaving gaps between
    // each element written.
    size_t iterations = 0;
    while (!cancel_flag_->IsSet()) {
      if (start_value_ == 1) {
        // Only write one value, otherwise there would be no gaps.
        void* ptr = reinterpret_cast<void*>(start_value_);
        filter_->Add(ptr);
      } else {
        for (uintptr_t value = start_value_; value <= max_value_;
             value += start_value_) {
          void* ptr = reinterpret_cast<void*>(value);
          filter_->Add(ptr);
        }
      }
      // Try to leave a good balance between contending to write and not
      // starving other writers.
      if (iterations++ % 100 == 0) {
        PlatformThread::YieldCurrentThread();
      }
    }
  }

 private:
  uintptr_t start_value_;
  uintptr_t max_value_;
  raw_ref<LockFreeBloomFilterType> filter_;
  raw_ref<base::TestWaitableEvent> all_started_event_;
  raw_ref<base::AtomicFlag> cancel_flag_;
  base::RepeatingClosure on_started_closure_;
};

}  // namespace

TEST(LockFreeBloomFilterTest, SingleHash) {
  LockFreeBloomFilter</*BitsPerKey=*/1, /*UseFakeHashFunctionsForTesting=*/true>
      filter;
  EXPECT_EQ(0, filter.CountBits());

  // See the chart above the kAlpha definition for expected hash results.
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kAlfa)),
            0x10'000);  // 2^16
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kBravo)),
            0x20'000);  // 2^17
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kCharlie)),
            0x40'000);  // 2^18
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kDelta)),
            0x100'000'000);  // 2^32
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kEcho)),
            0x10'000);  // 2^16

  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  filter.Add(reinterpret_cast<void*>(kAlfa));
  EXPECT_EQ(1, filter.CountBits());
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  // False positive: kAlfa and kEcho collide with hash function 0.
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  filter.Add(reinterpret_cast<void*>(kBravo));
  filter.Add(reinterpret_cast<void*>(kCharlie));
  EXPECT_EQ(3, filter.CountBits());

  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // Reset to only kAlfa.
  filter.AtomicSetBits(filter.GetBitsForKey(reinterpret_cast<void*>(kAlfa)));
  EXPECT_EQ(filter.GetBitsForTesting(), 0x10'000);
}

TEST(LockFreeBloomFilterTest, MultiHash) {
  LockFreeBloomFilter</*BitsPerKey=*/3, /*UseFakeHashFunctionsForTesting=*/true>
      filter;
  EXPECT_EQ(filter.CountBits(), 0);

  // See the chart above the kAlpha definition for expected hash results.
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kAlfa)),
            0x10'110);  // 2^16 | 2^8 | 2^4
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kBravo)),
            0x20'110);  // 2^17 | 2^8 | 2^4
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kCharlie)),
            0x40'210);  // 2^18 | 2^9 | 2^4
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kDelta)),
            0x100'010'100);  // 2^32 | 2^16 | 2^8
  EXPECT_EQ(filter.GetBitsForKey(reinterpret_cast<void*>(kEcho)),
            0x10'000'110'000);  // 2^16 | 2^40 | 2^20

  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // None of the pointers collide for all 3 hash functions, so there should be
  // no false positives.
  filter.Add(reinterpret_cast<void*>(kAlfa));
  EXPECT_EQ(filter.CountBits(), 3);
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // kBravo only sets 1 new bit because it collides with kAlfa for hash
  // functions 1 and 2.
  filter.Add(reinterpret_cast<void*>(kBravo));
  EXPECT_EQ(filter.CountBits(), 4);

  // kCharlie only sets 2 new bits because it collides with kAlfa and kBravo for
  // hash function 2.
  filter.Add(reinterpret_cast<void*>(kCharlie));
  EXPECT_EQ(filter.CountBits(), 6);

  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // Reset to only kAlfa.
  filter.AtomicSetBits(filter.GetBitsForKey(reinterpret_cast<void*>(kAlfa)));
  EXPECT_EQ(filter.GetBitsForTesting(), 0x10'110);  // 2^16 | 2^8 | 2^4
}

TEST(LockFreeBloomFilterTest, FalsePositivesWithSingleBitFilterCollisions) {
  LockFreeBloomFilter<1> filter;

  // Loop until a hash collision occurs. This is guaranteed to happen by the
  // time kMaxLockFreeBloomFilterBits keys are added.
  for (size_t i = 0; i <= kMaxLockFreeBloomFilterBits; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    if (filter.MaybeContains(ptr)) {
      // Hash collision occurred. Adding the new key should appear to succeed,
      // but change nothing.
      size_t bits_before = filter.CountBits();
      filter.Add(ptr);
      EXPECT_EQ(filter.CountBits(), bits_before);
      for (size_t j = 0; j <= i; ++j) {
        EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(j)));
      }
      // Test succeeded.
      return;
    }
    filter.Add(ptr);
    EXPECT_TRUE(filter.MaybeContains(ptr));
  }

  FAIL() << "Added " << kMaxLockFreeBloomFilterBits
         << " keys without a false positive";
}

TEST(LockFreeBloomFilterTest, EverythingMatches) {
  // Provide filter data with all bits set ON.
  LockFreeBloomFilter<5> filter;
  filter.AtomicSetBits(static_cast<LockFreeBloomFilterBits>(-1));

  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));
}

TEST(LockFreeBloomFilterTest, ConcurrentAccess) {
  // The purpose of this test is to make sure adding pointers concurrently
  // does not disrupt the state of other keys. Each writer races to set the bits
  // for a single key. To get a high amount of parallelism they set the bits in
  // many filters.
  WriterThread::LockFreeBloomFilterType expected_filter;

  // Add two dozen elements to `expected_filter`, in serial. Make sure this
  // doesn't saturate all the bits in the filter because that wouldn't be an
  // interesting test. (Should be impossible since at most 2 bits are set for
  // each value.)
  static constexpr uintptr_t kMaxElement = 24;
  for (uintptr_t value = 1; value <= kMaxElement; ++value) {
    void* ptr = reinterpret_cast<void*>(value);
    expected_filter.Add(ptr);
  }
  ASSERT_LT(expected_filter.CountBits(), kMaxLockFreeBloomFilterBits);

  WriterThread::LockFreeBloomFilterType filter;

  // Add the same elements to `filter`, in parallel, and expect the outcome
  // to be identical.
  base::TestWaitableEvent all_started_event;
  base::AtomicFlag cancel_flag;

  // One writer per value. Each writer will start at that value and fill in the
  // filter with every multiple of that value (except for 1 which only writes
  // itself), so the values are written in different orders depending on thread
  // timing.
  base::RepeatingClosure on_started_closure = base::BarrierClosure(
      kMaxElement, base::BindRepeating(&base::TestWaitableEvent::Signal,
                                       base::Unretained(&all_started_event)));
  std::vector<std::unique_ptr<WriterThread>> writers;
  for (uintptr_t value = 1; value <= kMaxElement; ++value) {
    writers.push_back(std::make_unique<WriterThread>(
        value, kMaxElement, filter, all_started_event, cancel_flag,
        on_started_closure));
    writers.back()->Start();
  }
  absl::Cleanup join_on_exit = [&cancel_flag, &writers] {
    cancel_flag.Set();
    for (const auto& writer_thread : writers) {
      writer_thread->Join();
    }
  };

  all_started_event.Wait();

  // Repeatedly wait until the filter matches `expected_filter`, then clear it
  // so the writers start again.
  static constexpr int kMaxLoops = 200;
  const auto start_time = base::TimeTicks::Now();
  for (int i = 0; i < kMaxLoops; ++i) {
    while (filter.GetBitsForTesting() != expected_filter.GetBitsForTesting()) {
      if (base::TimeTicks::Now() - start_time >=
          TestTimeouts::action_max_timeout()) {
        FAIL() << "Timed out after converging on expected_filter " << i
               << " out of " << kMaxLoops << " times";
      }
      // Don't starve the writer threads.
      PlatformThread::YieldCurrentThread();
    }
    filter.AtomicSetBits(0u);
  }
}

TEST(LockFreeBloomFilterTest, IndependentHashes) {
  LockFreeBloomFilter<3> filter;
  // 32-bit platforms only have room for 5 bits per key.
  LockFreeBloomFilter<sizeof(size_t) < 8 ? 5 : 8> filter2;

  std::vector<void*> ptrs;
  absl::Cleanup free_on_exit = [&ptrs] {
    for (void* ptr : ptrs) {
      free(ptr);
    }
  };

  absl::flat_hash_set<LockFreeBloomFilterBits> bit_patterns;
  absl::flat_hash_set<LockFreeBloomFilterBits> bit_patterns2;
  for (int i = 1; i <= 1000; ++i) {
    ptrs.push_back(malloc(64));
    bit_patterns.insert(filter.GetBitsForKey(ptrs.back()));
    bit_patterns2.insert(filter2.GetBitsForKey(ptrs.back()));
    ASSERT_GE(bit_patterns.size(), floor(i * 0.8))
        << i << " keys, " << bit_patterns.size() << " distinct bit patterns";
    ASSERT_GE(bit_patterns2.size(), floor(i * 0.8))
        << i << " keys, " << bit_patterns2.size() << " distinct bit patterns";
  }
}

}  // namespace base
