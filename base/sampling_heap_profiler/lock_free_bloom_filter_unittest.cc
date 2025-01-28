// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/lock_free_bloom_filter.h"

#include <stdint.h>

#include <bitset>
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
  WriterThread(uintptr_t start_value,
               uintptr_t max_value,
               LockFreeBloomFilter& filter,
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
  raw_ref<LockFreeBloomFilter> filter_;
  raw_ref<base::TestWaitableEvent> all_started_event_;
  raw_ref<base::AtomicFlag> cancel_flag_;
  base::RepeatingClosure on_started_closure_;
};

size_t CountBits(const LockFreeBloomFilter& filter) {
  return std::bitset<LockFreeBloomFilter::kMaxBits>(filter.GetBitsForTesting())
      .count();
}

}  // namespace

TEST(LockFreeBloomFilterTest, SingleHash) {
  LockFreeBloomFilter filter(/*num_hash_functions=*/1);
  filter.SetFakeHashFunctionsForTesting(true);
  EXPECT_EQ(0, CountBits(filter));

  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  filter.Add(reinterpret_cast<void*>(kAlfa));
  EXPECT_EQ(1, CountBits(filter));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  // False positive: kAlfa and kEcho collide with hash function 0.
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  filter.Add(reinterpret_cast<void*>(kBravo));
  filter.Add(reinterpret_cast<void*>(kCharlie));
  EXPECT_EQ(3, CountBits(filter));

  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));
}

TEST(LockFreeBloomFilterTest, MultiHash) {
  LockFreeBloomFilter filter(/*num_hash_functions=*/3);
  filter.SetFakeHashFunctionsForTesting(true);
  EXPECT_EQ(CountBits(filter), 0);

  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // None of the pointers collide for all 3 hash functions, so there should be
  // no false positives.
  filter.Add(reinterpret_cast<void*>(kAlfa));
  EXPECT_EQ(CountBits(filter), 3);
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));

  // kBravo only sets 1 new bit because it collides with kAlfa for hash
  // functions 1 and 2.
  filter.Add(reinterpret_cast<void*>(kBravo));
  EXPECT_EQ(CountBits(filter), 4);

  // kCharlie only sets 2 new bits because it collides with kAlfa and kBravo for
  // hash function 2.
  filter.Add(reinterpret_cast<void*>(kCharlie));
  EXPECT_EQ(CountBits(filter), 6);

  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kAlfa)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kBravo)));
  EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(kCharlie)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kDelta)));
  EXPECT_FALSE(filter.MaybeContains(reinterpret_cast<void*>(kEcho)));
}

TEST(LockFreeBloomFilterTest, FalsePositivesWithSingleBitFilterCollisions) {
  LockFreeBloomFilter filter(/*num_hash_functions=*/1);

  // Loop until a hash collision occurs. This is guaranteed to happen by the
  // time kMaxBits keys are added.
  for (size_t i = 0; i <= LockFreeBloomFilter::kMaxBits; ++i) {
    void* ptr = reinterpret_cast<void*>(i);
    if (filter.MaybeContains(ptr)) {
      // Hash collision occurred. Adding the new key should appear to succeed,
      // but change nothing.
      size_t bits_before = CountBits(filter);
      filter.Add(ptr);
      EXPECT_EQ(CountBits(filter), bits_before);
      for (size_t j = 0; j <= i; ++j) {
        EXPECT_TRUE(filter.MaybeContains(reinterpret_cast<void*>(j)));
      }
      // Test succeeded.
      return;
    }
    filter.Add(ptr);
    EXPECT_TRUE(filter.MaybeContains(ptr));
  }

  FAIL() << "Added " << LockFreeBloomFilter::kMaxBits
         << " keys without a false positive";
}

TEST(LockFreeBloomFilterTest, EverythingMatches) {
  // Provide filter data with all bits set ON.
  LockFreeBloomFilter filter(/*num_hash_functions=*/7);
  filter.SetBitsForTesting(static_cast<LockFreeBloomFilter::BitStorage>(-1));

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
  LockFreeBloomFilter expected_filter(/*num_hash_functions=*/2);

  // Add two dozen elements to `expected_filter`, in serial. Make sure this
  // doesn't saturate all the bits in the filter because that wouldn't be an
  // interesting test. (Should be impossible since at most 2 bits are set for
  // each value.)
  static constexpr uintptr_t kMaxElement = 24;
  for (uintptr_t value = 1; value <= kMaxElement; ++value) {
    void* ptr = reinterpret_cast<void*>(value);
    expected_filter.Add(ptr);
  }
  ASSERT_LT(CountBits(expected_filter), LockFreeBloomFilter::kMaxBits);

  LockFreeBloomFilter filter(/*num_hash_functions=*/2);

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
  static constexpr int kMaxLoops = 2000;
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
    filter.SetBitsForTesting(0u);
  }
}

}  // namespace base
