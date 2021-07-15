// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

#include <stdlib.h>
#include <cinttypes>

#include "base/allocator/allocator_shim.h"
#include "base/debug/alias.h"
#include "base/rand_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class SamplingHeapProfilerTest : public ::testing::Test {
 public:
  void SetUp() override {
#if defined(OS_APPLE)
    allocator::InitializeAllocatorShim();
#endif
    SamplingHeapProfiler::Init();
  }

  size_t GetNextSample(size_t mean_interval) {
    return PoissonAllocationSampler::GetNextSampleInterval(mean_interval);
  }

  static int GetRunningSessionsCount() {
    return SamplingHeapProfiler::Get()->running_sessions_;
  }

  static void RunStartStopLoop(SamplingHeapProfiler* profiler) {
    for (int i = 0; i < 100000; ++i) {
      profiler->Start();
      EXPECT_LE(1, GetRunningSessionsCount());
      profiler->Stop();
    }
  }
};

class SamplesCollector : public PoissonAllocationSampler::SamplesObserver {
 public:
  explicit SamplesCollector(size_t watch_size) : watch_size_(watch_size) {}

  void SampleAdded(void* address,
                   size_t size,
                   size_t,
                   PoissonAllocationSampler::AllocatorType,
                   const char*) override {
    if (sample_added || size != watch_size_)
      return;
    sample_address_ = address;
    sample_added = true;
  }

  void SampleRemoved(void* address) override {
    if (address == sample_address_)
      sample_removed = true;
  }

  bool sample_added = false;
  bool sample_removed = false;

 private:
  size_t watch_size_;
  void* sample_address_ = nullptr;
};

TEST_F(SamplingHeapProfilerTest, SampleObserver) {
  SamplesCollector collector(10000);
  auto* sampler = PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->SetSamplingInterval(1024);
  sampler->AddSamplesObserver(&collector);
  void* volatile p = malloc(10000);
  free(p);
  sampler->RemoveSamplesObserver(&collector);
  EXPECT_TRUE(collector.sample_added);
  EXPECT_TRUE(collector.sample_removed);
}

TEST_F(SamplingHeapProfilerTest, SampleObserverMuted) {
  SamplesCollector collector(10000);
  auto* sampler = PoissonAllocationSampler::Get();
  sampler->SuppressRandomnessForTest(true);
  sampler->SetSamplingInterval(1024);
  sampler->AddSamplesObserver(&collector);
  {
    PoissonAllocationSampler::ScopedMuteThreadSamples muted_scope;
    void* volatile p = malloc(10000);
    free(p);
  }
  sampler->RemoveSamplesObserver(&collector);
  EXPECT_FALSE(collector.sample_added);
  EXPECT_FALSE(collector.sample_removed);
}

TEST_F(SamplingHeapProfilerTest, IntervalRandomizationSanity) {
  PoissonAllocationSampler::Get()->SuppressRandomnessForTest(false);
  constexpr int iterations = 50;
  constexpr size_t target = 10000000;
  int sum = 0;
  for (int i = 0; i < iterations; ++i) {
    int samples = 0;
    for (size_t value = 0; value < target; value += GetNextSample(10000))
      ++samples;
    // There are should be ~ target/10000 = 1000 samples.
    sum += samples;
  }
  int mean_samples = sum / iterations;
  EXPECT_NEAR(1000, mean_samples, 100);  // 10% tolerance.
}

#if defined(OS_IOS)
// iOS devices generally have ~4GB of RAM with no swap and therefore need a
// lower allocation limit here.
const int kNumberOfAllocations = 1000;
#else
const int kNumberOfAllocations = 10000;
#endif

NOINLINE void Allocate1() {
  void* p = malloc(400);
  base::debug::Alias(&p);
}

NOINLINE void Allocate2() {
  void* p = malloc(700);
  base::debug::Alias(&p);
}

NOINLINE void Allocate3() {
  void* p = malloc(20480);
  base::debug::Alias(&p);
}

class MyThread1 : public SimpleThread {
 public:
  MyThread1() : SimpleThread("MyThread1") {}
  void Run() override {
    for (int i = 0; i < kNumberOfAllocations; ++i)
      Allocate1();
  }
};

class MyThread2 : public SimpleThread {
 public:
  MyThread2() : SimpleThread("MyThread2") {}
  void Run() override {
    for (int i = 0; i < kNumberOfAllocations; ++i)
      Allocate2();
  }
};

void CheckAllocationPattern(void (*allocate_callback)()) {
  auto* profiler = SamplingHeapProfiler::Get();
  PoissonAllocationSampler::Get()->SuppressRandomnessForTest(false);
  profiler->SetSamplingInterval(10240);
  base::TimeTicks t0 = base::TimeTicks::Now();
  std::map<size_t, size_t> sums;
  const int iterations = 40;
  for (int i = 0; i < iterations; ++i) {
    uint32_t id = profiler->Start();
    allocate_callback();
    std::vector<SamplingHeapProfiler::Sample> samples =
        profiler->GetSamples(id);
    profiler->Stop();
    std::map<size_t, size_t> buckets;
    for (auto& sample : samples) {
      buckets[sample.size] += sample.total;
    }
    for (auto& it : buckets) {
      if (it.first != 400 && it.first != 700 && it.first != 20480)
        continue;
      sums[it.first] += it.second;
      printf("%zu,", it.second);
    }
    printf("\n");
  }

  printf("Time taken %" PRIu64 "ms\n",
         (base::TimeTicks::Now() - t0).InMilliseconds());

  for (auto sum : sums) {
    intptr_t expected = sum.first * kNumberOfAllocations;
    intptr_t actual = sum.second / iterations;
    printf("%zu:\tmean: %zu\trelative error: %.2f%%\n", sum.first, actual,
           100. * (actual - expected) / expected);
  }
}

// Manual tests to check precision of the sampling profiler.
// Yes, they do leak lots of memory.

TEST_F(SamplingHeapProfilerTest, DISABLED_ParallelLargeSmallStats) {
  CheckAllocationPattern([]() {
    MyThread1 t1;
    MyThread1 t2;
    t1.Start();
    t2.Start();
    for (int i = 0; i < kNumberOfAllocations; ++i)
      Allocate3();
    t1.Join();
    t2.Join();
  });
}

TEST_F(SamplingHeapProfilerTest, DISABLED_SequentialLargeSmallStats) {
  CheckAllocationPattern([]() {
    for (int i = 0; i < kNumberOfAllocations; ++i) {
      Allocate1();
      Allocate2();
      Allocate3();
    }
  });
}

// Platform TLS: alloc+free[ns]: 22.184  alloc[ns]: 8.910  free[ns]: 13.274
// thread_local: alloc+free[ns]: 18.353  alloc[ns]: 5.021  free[ns]: 13.331
// TODO(crbug.com/1117342) Disabled on Mac
#if defined(OS_MAC)
#define MAYBE_MANUAL_SamplerMicroBenchmark DISABLED_MANUAL_SamplerMicroBenchmark
#else
#define MAYBE_MANUAL_SamplerMicroBenchmark MANUAL_SamplerMicroBenchmark
#endif
TEST_F(SamplingHeapProfilerTest, MAYBE_MANUAL_SamplerMicroBenchmark) {
  // With the sampling interval of 100KB it happens to record ~ every 450th
  // allocation in the browser process. We model this pattern here.
  constexpr size_t sampling_interval = 100000;
  constexpr size_t allocation_size = sampling_interval / 450;
  SamplesCollector collector(0);
  auto* sampler = PoissonAllocationSampler::Get();
  sampler->SetSamplingInterval(sampling_interval);
  sampler->AddSamplesObserver(&collector);
  int kNumAllocations = 50000000;

  base::TimeTicks t0 = base::TimeTicks::Now();
  for (int i = 1; i <= kNumAllocations; ++i) {
    sampler->RecordAlloc(
        reinterpret_cast<void*>(static_cast<intptr_t>(i)), allocation_size,
        PoissonAllocationSampler::AllocatorType::kMalloc, nullptr);
  }
  base::TimeTicks t1 = base::TimeTicks::Now();
  for (int i = 1; i <= kNumAllocations; ++i)
    sampler->RecordFree(reinterpret_cast<void*>(static_cast<intptr_t>(i)));
  base::TimeTicks t2 = base::TimeTicks::Now();

  printf(
      "alloc+free[ns]: %.3f   alloc[ns]: %.3f   free[ns]: %.3f   "
      "alloc+free[mln/s]: %.1f   total[ms]: %.1f\n",
      (t2 - t0).InNanoseconds() * 1. / kNumAllocations,
      (t1 - t0).InNanoseconds() * 1. / kNumAllocations,
      (t2 - t1).InNanoseconds() * 1. / kNumAllocations,
      kNumAllocations / (t2 - t0).InMicrosecondsF(),
      (t2 - t0).InMillisecondsF());

  sampler->RemoveSamplesObserver(&collector);
}

class StartStopThread : public SimpleThread {
 public:
  StartStopThread(WaitableEvent* event)
      : SimpleThread("MyThread2"), event_(event) {}
  void Run() override {
    auto* profiler = SamplingHeapProfiler::Get();
    event_->Signal();
    SamplingHeapProfilerTest::RunStartStopLoop(profiler);
  }

 private:
  WaitableEvent* event_;
};

TEST_F(SamplingHeapProfilerTest, StartStop) {
  auto* profiler = SamplingHeapProfiler::Get();
  EXPECT_EQ(0, GetRunningSessionsCount());
  profiler->Start();
  EXPECT_EQ(1, GetRunningSessionsCount());
  profiler->Start();
  EXPECT_EQ(2, GetRunningSessionsCount());
  profiler->Stop();
  EXPECT_EQ(1, GetRunningSessionsCount());
  profiler->Stop();
  EXPECT_EQ(0, GetRunningSessionsCount());
}

TEST_F(SamplingHeapProfilerTest, ConcurrentStartStop) {
  auto* profiler = SamplingHeapProfiler::Get();
  WaitableEvent event;
  StartStopThread thread(&event);
  thread.Start();
  event.Wait();
  RunStartStopLoop(profiler);
  thread.Join();
  EXPECT_EQ(0, GetRunningSessionsCount());
}

}  // namespace base
