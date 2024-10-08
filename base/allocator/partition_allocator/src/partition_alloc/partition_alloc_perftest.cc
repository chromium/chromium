// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc.h"

#include <algorithm>
#include <atomic>
#include <limits>
#include <memory>
#include <vector>

#include "base/debug/debugging_buildflags.h"
#include "base/timer/lap_timer.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/strings/stringprintf.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread_for_testing.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/thread_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(PA_ARCH_CPU_32_BITS) || \
    PA_BUILDFLAG(IS_FUCHSIA)
// Some tests allocate many GB of memory, which can cause issues on Android and
// address-space exhaustion for any 32-bit process.
#define MEMORY_CONSTRAINED
#endif

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
#include "base/allocator/dispatcher/dispatcher.h"
#include "base/debug/allocation_trace.h"
#endif

namespace partition_alloc::internal {

namespace {

// Change kTimeLimit to something higher if you need more time to capture a
// trace.
constexpr ::base::TimeDelta kTimeLimit = ::base::Seconds(2);
constexpr int kWarmupRuns = 10000;
constexpr int kTimeCheckInterval = 100000;
constexpr size_t kAllocSize = 40;

// Size constants are mostly arbitrary, but try to simulate something like CSS
// parsing which consists of lots of relatively small objects.
constexpr int kMultiBucketMinimumSize = 24;
constexpr int kMultiBucketIncrement = 13;
// Final size is 24 + (13 * 22) = 310 bytes.
constexpr int kMultiBucketRounds = 22;

constexpr char kMetricPrefixMemoryAllocation[] = "MemoryAllocation.";
constexpr char kMetricThroughput[] = "throughput";
constexpr char kMetricTimePerAllocation[] = "time_per_allocation";

perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
  perf_test::PerfResultReporter reporter(kMetricPrefixMemoryAllocation,
                                         story_name);
  reporter.RegisterImportantMetric(kMetricThroughput, "runs/s");
  reporter.RegisterImportantMetric(kMetricTimePerAllocation, "ns");
  return reporter;
}

enum class AllocatorType {
  kSystem,
  kPartitionAlloc,
  kPartitionAllocWithThreadCache,
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
  kPartitionAllocWithAllocationStackTraceRecorder,
#endif
};

class Allocator {
 public:
  Allocator() = default;
  virtual ~Allocator() = default;
  virtual void* Alloc(size_t size) = 0;
  virtual void Free(void* data) = 0;
};

class SystemAllocator : public Allocator {
 public:
  SystemAllocator() = default;
  ~SystemAllocator() override = default;
  void* Alloc(size_t size) override { return malloc(size); }
  void Free(void* data) override { free(data); }
};

class PartitionAllocator : public Allocator {
 public:
  PartitionAllocator() = default;
  ~PartitionAllocator() override { alloc_.DestructForTesting(); }

  void* Alloc(size_t size) override {
    return alloc_.AllocInline<AllocFlags::kNoHooks>(size);
  }
  void Free(void* data) override {
    // Even though it's easy to invoke the fast path with
    // alloc_.Free<kNoHooks>(), we chose to use the slower path, because it's
    // more common with PA-E.
    PartitionRoot::FreeInlineInUnknownRoot<
        partition_alloc::FreeFlags::kNoHooks>(data);
  }

 private:
  PartitionRoot alloc_{PartitionOptions{}};
};

class PartitionAllocatorWithThreadCache : public Allocator {
 public:
  explicit PartitionAllocatorWithThreadCache(bool use_alternate_bucket_dist)
      : scope_(allocator_.root()) {
    ThreadCacheRegistry::Instance().PurgeAll();
    if (!use_alternate_bucket_dist) {
      allocator_.root()->SwitchToDenserBucketDistribution();
    } else {
      allocator_.root()->ResetBucketDistributionForTesting();
    }
  }
  ~PartitionAllocatorWithThreadCache() override = default;

  void* Alloc(size_t size) override {
    return allocator_.root()->AllocInline<AllocFlags::kNoHooks>(size);
  }
  void Free(void* data) override {
    // Even though it's easy to invoke the fast path with
    // alloc_.Free<kNoHooks>(), we chose to use the slower path, because it's
    // more common with PA-E.
    PartitionRoot::FreeInlineInUnknownRoot<
        partition_alloc::FreeFlags::kNoHooks>(data);
  }

 private:
  static constexpr partition_alloc::PartitionOptions kOpts = [] {
    partition_alloc::PartitionOptions opts;
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    opts.thread_cache = PartitionOptions::kEnabled;
#endif
    return opts;
  }();
  PartitionAllocatorForTesting<internal::DisallowLeaks> allocator_{kOpts};
  internal::ThreadCacheProcessScopeForTesting scope_;
};

#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
class PartitionAllocatorWithAllocationStackTraceRecorder : public Allocator {
 public:
  explicit PartitionAllocatorWithAllocationStackTraceRecorder(
      bool register_hooks)
      : register_hooks_(register_hooks) {
    if (register_hooks_) {
      dispatcher_.InitializeForTesting(&recorder_);
    }
  }

  ~PartitionAllocatorWithAllocationStackTraceRecorder() override {
    if (register_hooks_) {
      dispatcher_.ResetForTesting();
    }
  }

  void* Alloc(size_t size) override { return alloc_.AllocInline(size); }

  void Free(void* data) override {
    // Even though it's easy to invoke the fast path with
    // alloc_.Free<kNoHooks>(), we chose to use the slower path, because it's
    // more common with PA-E.
    PartitionRoot::FreeInlineInUnknownRoot<
        partition_alloc::FreeFlags::kNoHooks>(data);
  }

 private:
  bool const register_hooks_;
  PartitionRoot alloc_{PartitionOptions{}};
  ::base::allocator::dispatcher::Dispatcher& dispatcher_ =
      ::base::allocator::dispatcher::Dispatcher::GetInstance();
  ::base::debug::tracer::AllocationTraceRecorder recorder_;
};
#endif  // BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)

class TestLoopThread : public base::PlatformThreadForTesting::Delegate {
 public:
  TestLoopThread(float (*test_fn)(Allocator*), Allocator* allocator)
      : test_fn_(test_fn), allocator_(allocator) {
    PA_CHECK(base::PlatformThreadForTesting::Create(0, this, &thread_handle_));
  }

  float Run() {
    base::PlatformThreadForTesting::Join(thread_handle_);
    return laps_per_second_;
  }

  void ThreadMain() override { laps_per_second_ = test_fn_(allocator_); }

  float (*test_fn_)(Allocator*) = nullptr;
  Allocator* allocator_ = nullptr;
  base::PlatformThreadHandle thread_handle_;
  std::atomic<float> laps_per_second_;
};

void DisplayResults(const std::string& story_name,
                    float iterations_per_second) {
  auto reporter = SetUpReporter(story_name);
  reporter.AddResult(kMetricThroughput, iterations_per_second);
  reporter.AddResult(kMetricTimePerAllocation,
                     static_cast<size_t>(1e9 / iterations_per_second));
}

class MemoryAllocationPerfNode {
 public:
  MemoryAllocationPerfNode* GetNext() const { return next_; }
  void SetNext(MemoryAllocationPerfNode* p) { next_ = p; }
  static void FreeAll(MemoryAllocationPerfNode* first, Allocator* alloc) {
    MemoryAllocationPerfNode* cur = first;
    while (cur != nullptr) {
      MemoryAllocationPerfNode* next = cur->GetNext();
      alloc->Free(cur);
      cur = next;
    }
  }

 private:
  MemoryAllocationPerfNode* next_ = nullptr;
};

#if !defined(MEMORY_CONSTRAINED)
float SingleBucket(Allocator* allocator) {
  auto* first =
      reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(kAllocSize));
  size_t allocated_memory = kAllocSize;

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  MemoryAllocationPerfNode* cur = first;
  do {
    auto* next = reinterpret_cast<MemoryAllocationPerfNode*>(
        allocator->Alloc(kAllocSize));
    PA_CHECK(next != nullptr);
    cur->SetNext(next);
    cur = next;
    timer.NextLap();
    allocated_memory += kAllocSize;
    // With multiple threads, can get OOM otherwise.
    if (allocated_memory > 200e6) {
      cur->SetNext(nullptr);
      MemoryAllocationPerfNode::FreeAll(first->GetNext(), allocator);
      cur = first;
      allocated_memory = kAllocSize;
    }
  } while (!timer.HasTimeLimitExpired());

  // next_ = nullptr only works if the class constructor is called (it's not
  // called in this case because then we can allocate arbitrary-length
  // payloads.)
  cur->SetNext(nullptr);
  MemoryAllocationPerfNode::FreeAll(first, allocator);

  return timer.LapsPerSecond();
}
#endif  // defined(MEMORY_CONSTRAINED)

float SingleBucketWithFree(Allocator* allocator) {
  // Allocate an initial element to make sure the bucket stays set up.
  void* elem = allocator->Alloc(kAllocSize);

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    void* cur = allocator->Alloc(kAllocSize);
    PA_CHECK(cur != nullptr);
    allocator->Free(cur);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  allocator->Free(elem);
  return timer.LapsPerSecond();
}

#if !defined(MEMORY_CONSTRAINED)
float MultiBucket(Allocator* allocator) {
  auto* first =
      reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(kAllocSize));
  MemoryAllocationPerfNode* cur = first;
  size_t allocated_memory = kAllocSize;

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    for (int i = 0; i < kMultiBucketRounds; i++) {
      size_t size = kMultiBucketMinimumSize + (i * kMultiBucketIncrement);
      auto* next =
          reinterpret_cast<MemoryAllocationPerfNode*>(allocator->Alloc(size));
      PA_CHECK(next != nullptr);
      cur->SetNext(next);
      cur = next;
      allocated_memory += size;
    }

    // Can OOM with multiple threads.
    if (allocated_memory > 100e6) {
      cur->SetNext(nullptr);
      MemoryAllocationPerfNode::FreeAll(first->GetNext(), allocator);
      cur = first;
      allocated_memory = kAllocSize;
    }

    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  cur->SetNext(nullptr);
  MemoryAllocationPerfNode::FreeAll(first, allocator);

  return timer.LapsPerSecond() * kMultiBucketRounds;
}
#endif  // defined(MEMORY_CONSTRAINED)

float MultiBucketWithFree(Allocator* allocator) {
  std::vector<void*> elems;
  elems.reserve(kMultiBucketRounds);
  // Do an initial round of allocation to make sure that the buckets stay in
  // use (and aren't accidentally released back to the OS).
  for (int i = 0; i < kMultiBucketRounds; i++) {
    void* cur =
        allocator->Alloc(kMultiBucketMinimumSize + (i * kMultiBucketIncrement));
    PA_CHECK(cur != nullptr);
    elems.push_back(cur);
  }

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    for (int i = 0; i < kMultiBucketRounds; i++) {
      void* cur = allocator->Alloc(kMultiBucketMinimumSize +
                                   (i * kMultiBucketIncrement));
      PA_CHECK(cur != nullptr);
      allocator->Free(cur);
    }
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  for (void* ptr : elems) {
    allocator->Free(ptr);
  }

  return timer.LapsPerSecond() * kMultiBucketRounds;
}

float DirectMapped(Allocator* allocator) {
  constexpr size_t kSize = 2 * 1000 * 1000;

  ::base::LapTimer timer(kWarmupRuns, kTimeLimit, kTimeCheckInterval);
  do {
    void* cur = allocator->Alloc(kSize);
    PA_CHECK(cur != nullptr);
    allocator->Free(cur);
    timer.NextLap();
  } while (!timer.HasTimeLimitExpired());

  return timer.LapsPerSecond();
}

std::unique_ptr<Allocator> CreateAllocator(AllocatorType type,
                                           bool use_alternate_bucket_dist) {
  switch (type) {
    case AllocatorType::kSystem:
      return std::make_unique<SystemAllocator>();
    case AllocatorType::kPartitionAlloc:
      return std::make_unique<PartitionAllocator>();
    case AllocatorType::kPartitionAllocWithThreadCache:
      return std::make_unique<PartitionAllocatorWithThreadCache>(
          use_alternate_bucket_dist);
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
    case AllocatorType::kPartitionAllocWithAllocationStackTraceRecorder:
      return std::make_unique<
          PartitionAllocatorWithAllocationStackTraceRecorder>(true);
#endif
  }
}

void LogResults(int thread_count,
                AllocatorType alloc_type,
                uint64_t total_laps_per_second,
                uint64_t min_laps_per_second) {
  PA_LOG(INFO) << "RESULTSCSV: " << thread_count << ","
               << static_cast<int>(alloc_type) << "," << total_laps_per_second
               << "," << min_laps_per_second;
}

void RunTest(int thread_count,
             bool use_alternate_bucket_dist,
             AllocatorType alloc_type,
             float (*test_fn)(Allocator*),
             float (*noisy_neighbor_fn)(Allocator*),
             const char* story_base_name) {
  auto alloc = CreateAllocator(alloc_type, use_alternate_bucket_dist);

  std::unique_ptr<TestLoopThread> noisy_neighbor_thread = nullptr;
  if (noisy_neighbor_fn) {
    noisy_neighbor_thread =
        std::make_unique<TestLoopThread>(noisy_neighbor_fn, alloc.get());
  }

  std::vector<std::unique_ptr<TestLoopThread>> threads;
  for (int i = 0; i < thread_count; ++i) {
    threads.push_back(std::make_unique<TestLoopThread>(test_fn, alloc.get()));
  }

  uint64_t total_laps_per_second = 0;
  uint64_t min_laps_per_second = std::numeric_limits<uint64_t>::max();
  for (int i = 0; i < thread_count; ++i) {
    uint64_t laps_per_second = threads[i]->Run();
    min_laps_per_second = std::min(laps_per_second, min_laps_per_second);
    total_laps_per_second += laps_per_second;
  }

  if (noisy_neighbor_thread) {
    noisy_neighbor_thread->Run();
  }

  char const* alloc_type_str;
  switch (alloc_type) {
    case AllocatorType::kSystem:
      alloc_type_str = "System";
      break;
    case AllocatorType::kPartitionAlloc:
      alloc_type_str = "PartitionAlloc";
      break;
    case AllocatorType::kPartitionAllocWithThreadCache:
      alloc_type_str = "PartitionAllocWithThreadCache";
      break;
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
    case AllocatorType::kPartitionAllocWithAllocationStackTraceRecorder:
      alloc_type_str = "PartitionAllocWithAllocationStackTraceRecorder";
      break;
#endif
  }

  std::string name = base::TruncatingStringPrintf(
      "%s%s_%s_%d", kMetricPrefixMemoryAllocation, story_base_name,
      alloc_type_str, thread_count);

  DisplayResults(name + "_total", total_laps_per_second);
  DisplayResults(name + "_worst", min_laps_per_second);
  LogResults(thread_count, alloc_type, total_laps_per_second,
             min_laps_per_second);
}

class PartitionAllocMemoryAllocationPerfTest
    : public testing::TestWithParam<std::tuple<int, bool, AllocatorType>> {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  void SetUp() override {
    PartitionRoot::EnableShadowMetadata(
        partition_alloc::internal::PoolHandleMask::kRegular |
        partition_alloc::internal::PoolHandleMask::kBRP);
  }
#endif
};

// Only one partition with a thread cache: cannot use the thread cache when
// PartitionAlloc is malloc().
INSTANTIATE_TEST_SUITE_P(
    ,
    PartitionAllocMemoryAllocationPerfTest,
    ::testing::Combine(
        ::testing::Values(1, 2, 3, 4),
        ::testing::Values(false, true),
        ::testing::Values(
            AllocatorType::kSystem,
            AllocatorType::kPartitionAlloc,
            AllocatorType::kPartitionAllocWithThreadCache
#if BUILDFLAG(ENABLE_ALLOCATION_STACK_TRACE_RECORDER)
            ,
            AllocatorType::kPartitionAllocWithAllocationStackTraceRecorder
#endif
            )));

// This test (and the other one below) allocates a large amount of memory, which
// can cause issues on Android.
#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest, SingleBucket) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), SingleBucket, nullptr,
          "SingleBucket");
}
#endif  // defined(MEMORY_CONSTRAINED)

TEST_P(PartitionAllocMemoryAllocationPerfTest, SingleBucketWithFree) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), SingleBucketWithFree, nullptr,
          "SingleBucketWithFree");
}

#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest, MultiBucket) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucket, nullptr, "MultiBucket");
}
#endif  // defined(MEMORY_CONSTRAINED)

TEST_P(PartitionAllocMemoryAllocationPerfTest, MultiBucketWithFree) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucketWithFree, nullptr,
          "MultiBucketWithFree");
}

TEST_P(PartitionAllocMemoryAllocationPerfTest, DirectMapped) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), DirectMapped, nullptr,
          "DirectMapped");
}

#if !defined(MEMORY_CONSTRAINED)
TEST_P(PartitionAllocMemoryAllocationPerfTest,
       DISABLED_MultiBucketWithNoisyNeighbor) {
  auto params = GetParam();
  RunTest(std::get<int>(params), std::get<bool>(params),
          std::get<AllocatorType>(params), MultiBucket, DirectMapped,
          "MultiBucketWithNoisyNeighbor");
}
#endif  // !defined(MEMORY_CONSTRAINED)

}  // namespace

}  // namespace partition_alloc::internal
