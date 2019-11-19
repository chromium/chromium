// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <vector>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {
namespace {

// Change kTimeLimit to something higher if you need more time to capture a
// trace.
constexpr base::TimeDelta kTimeLimit = base::TimeDelta::FromSeconds(2);
constexpr int kWarmupRuns = 5;
constexpr int kTimeCheckInterval = 100000;

// Size constants are mostly arbitrary, but try to simulate something like CSS
// parsing which consists of lots of relatively small objects.
constexpr int kMultiBucketMinimumSize = 24;
constexpr int kMultiBucketIncrement = 13;
// Final size is 24 + (13 * 22) = 310 bytes.
constexpr int kMultiBucketRounds = 22;

class AllocatingThread : public PlatformThread::Delegate {
 public:
  explicit AllocatingThread(PartitionAllocatorGeneric* allocator)
      : allocator_(allocator), should_stop_(false) {
    PlatformThread::Create(0, this, &thread_handle_);
  }

  ~AllocatingThread() override {
    should_stop_ = true;
    PlatformThread::Join(thread_handle_);
  }

  // Allocates and frees memory in a loop until |should_stop_| becomes true.
  void ThreadMain() override {
    uint64_t count = 0;
    while (true) {
      // Only check |should_stop_| every 2^15 iterations, as it is a
      // sequentially consistent access, hence expensive.
      if (count % (1 << 15) == 0 && should_stop_)
        break;
      void* data = allocator_->root()->Alloc(10, "");
      allocator_->root()->Free(data);
      count++;
    }
  }

  PartitionAllocatorGeneric* allocator_;
  std::atomic<bool> should_stop_;
  PlatformThreadHandle thread_handle_;
};

void DisplayResults(const std::string& measurement,
                    const std::string& modifier,
                    size_t iterations_per_second) {
  perf_test::PrintResult(measurement, modifier, "", iterations_per_second,
                         "runs/s", true);
  perf_test::PrintResult(measurement, modifier, "",
                         static_cast<size_t>(1e9 / iterations_per_second),
                         "ns/run", true);
}

class MemoryAllocationPerfNode {
 public:
  MemoryAllocationPerfNode* GetNext() const { return next_; }
  void SetNext(MemoryAllocationPerfNode* p) { next_ = p; }
  static void FreeAll(MemoryAllocationPerfNode* first,
                      PartitionAllocatorGeneric& alloc) {
    MemoryAllocationPerfNode* cur = first;
    while (cur != nullptr) {
      MemoryAllocationPerfNode* next = cur->GetNext();
      alloc.root()->Free(cur);
      cur = next;
    }
  }

 private:
  MemoryAllocationPerfNode* next_ = nullptr;
};

class MemoryAllocationPerfTest : public testing::Test {
 public:
  MemoryAllocationPerfTest()
      : timer_(kWarmupRuns, kTimeLimit, kTimeCheckInterval) {}
  void SetUp() override { alloc_.init(); }
  void TearDown() override {
    alloc_.root()->PurgeMemory(PartitionPurgeDecommitEmptyPages |
                               PartitionPurgeDiscardUnusedSystemPages);
  }

 protected:
  void TestSingleBucket() {
    MemoryAllocationPerfNode* first =
        reinterpret_cast<MemoryAllocationPerfNode*>(
            alloc_.root()->Alloc(40, "<testing>"));

    timer_.Reset();
    MemoryAllocationPerfNode* cur = first;
    do {
      MemoryAllocationPerfNode* next =
          reinterpret_cast<MemoryAllocationPerfNode*>(
              alloc_.root()->Alloc(40, "<testing>"));
      CHECK_NE(next, nullptr);
      cur->SetNext(next);
      cur = next;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
    // next_ = nullptr only works if the class constructor is called (it's not
    // called in this case because then we can allocate arbitrary-length
    // payloads.)
    cur->SetNext(nullptr);

    MemoryAllocationPerfNode::FreeAll(first, alloc_);

    DisplayResults("MemoryAllocationPerfTest",
                   " single bucket allocation (40 bytes)",
                   timer_.LapsPerSecond());
  }

  void TestSingleBucketWithFree() {
    // Allocate an initial element to make sure the bucket stays set up.
    void* elem = alloc_.root()->Alloc(40, "<testing>");

    timer_.Reset();
    do {
      void* cur = alloc_.root()->Alloc(40, "<testing>");
      CHECK_NE(cur, nullptr);
      alloc_.root()->Free(cur);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    alloc_.root()->Free(elem);
    DisplayResults("MemoryAllocationPerfTest",
                   " single bucket allocation + free (40 bytes)",
                   timer_.LapsPerSecond());
  }

  void TestMultiBucket() {
    MemoryAllocationPerfNode* first =
        reinterpret_cast<MemoryAllocationPerfNode*>(
            alloc_.root()->Alloc(40, "<testing>"));
    MemoryAllocationPerfNode* cur = first;

    timer_.Reset();
    do {
      for (int i = 0; i < kMultiBucketRounds; i++) {
        MemoryAllocationPerfNode* next =
            reinterpret_cast<MemoryAllocationPerfNode*>(alloc_.root()->Alloc(
                kMultiBucketMinimumSize + (i * kMultiBucketIncrement),
                "<testing>"));
        CHECK_NE(next, nullptr);
        cur->SetNext(next);
        cur = next;
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());
    cur->SetNext(nullptr);

    MemoryAllocationPerfNode::FreeAll(first, alloc_);

    DisplayResults("MemoryAllocationPerfTest", " multi-bucket allocation",
                   timer_.LapsPerSecond() * kMultiBucketRounds);
  }

  void TestMultiBucketWithFree() {
    std::vector<void*> elems;
    elems.reserve(kMultiBucketRounds);
    // Do an initial round of allocation to make sure that the buckets stay in
    // use (and aren't accidentally released back to the OS).
    for (int i = 0; i < kMultiBucketRounds; i++) {
      void* cur = alloc_.root()->Alloc(
          kMultiBucketMinimumSize + (i * kMultiBucketIncrement), "<testing>");
      CHECK_NE(cur, nullptr);
      elems.push_back(cur);
    }

    timer_.Reset();
    do {
      for (int i = 0; i < kMultiBucketRounds; i++) {
        void* cur = alloc_.root()->Alloc(
            kMultiBucketMinimumSize + (i * kMultiBucketIncrement), "<testing>");
        CHECK_NE(cur, nullptr);
        alloc_.root()->Free(cur);
      }
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    for (void* ptr : elems) {
      alloc_.root()->Free(ptr);
    }

    DisplayResults("MemoryAllocationPerfTest",
                   " multi-bucket allocation + free",
                   timer_.LapsPerSecond() * kMultiBucketRounds);
  }

  LapTimer timer_;
  PartitionAllocatorGeneric alloc_;
};

TEST_F(MemoryAllocationPerfTest, SingleBucket) {
  TestSingleBucket();
}

TEST_F(MemoryAllocationPerfTest, SingleBucketWithCompetingThread) {
  AllocatingThread t(&alloc_);
  TestSingleBucket();
}

TEST_F(MemoryAllocationPerfTest, SingleBucketWithFree) {
  TestSingleBucketWithFree();
}

TEST_F(MemoryAllocationPerfTest, SingleBucketWithFreeWithCompetingThread) {
  AllocatingThread t(&alloc_);
  TestSingleBucketWithFree();
}

// Failing on Nexus5x: crbug.com/949838
#if defined(OS_ANDROID)
#define MAYBE_MultiBucket DISABLED_MultiBucket
#define MAYBE_MultiBucketWithCompetingThread \
  DISABLED_MultiBucketWithCompetingThread
#else
#define MAYBE_MultiBucket MultiBucket
#define MAYBE_MultiBucketWithCompetingThread MultiBucketWithCompetingThread
#endif
TEST_F(MemoryAllocationPerfTest, MAYBE_MultiBucket) {
  TestMultiBucket();
}

TEST_F(MemoryAllocationPerfTest, MAYBE_MultiBucketWithCompetingThread) {
  AllocatingThread t(&alloc_);
  TestMultiBucket();
}

TEST_F(MemoryAllocationPerfTest, MultiBucketWithFree) {
  TestMultiBucketWithFree();
}

TEST_F(MemoryAllocationPerfTest, MultiBucketWithFreeWithCompetingThread) {
  AllocatingThread t(&alloc_);
  TestMultiBucketWithFree();
}

}  // anonymous namespace

}  // namespace base
