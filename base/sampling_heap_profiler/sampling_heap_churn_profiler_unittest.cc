// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sampling_heap_profiler/sampling_heap_churn_profiler.h"

#include <stdlib.h>

#include "base/allocator/dispatcher/dispatcher.h"
#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "build/build_config.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using ScopedSuppressRandomnessForTesting =
    PoissonAllocationSampler::ScopedSuppressRandomnessForTesting;
using base::allocator::dispatcher::AllocationNotificationData;
using base::allocator::dispatcher::AllocationSubsystem;
using base::allocator::dispatcher::FreeNotificationData;

class SamplingHeapChurnProfilerTest : public ::testing::Test {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_APPLE)
    allocator_shim::InitializeAllocatorShim();
#endif
    SamplingHeapProfiler::Init();

    ASSERT_FALSE(PoissonAllocationSampler::AreHookedSamplesMuted());
    ASSERT_FALSE(PoissonAllocationSampler::ScopedMuteThreadSamples::IsMuted());
    ASSERT_FALSE(ScopedSuppressRandomnessForTesting::IsSuppressed());

    allocator::dispatcher::Dispatcher::GetInstance().InitializeForTesting(
        PoissonAllocationSampler::Get());
  }

  void TearDown() override {
    allocator::dispatcher::Dispatcher::GetInstance().ResetForTesting();
  }
};

TEST_F(SamplingHeapChurnProfilerTest, StartStop) {
  auto& churn_profiler = SamplingHeapProfiler::Get()->churn_profiler();
  churn_profiler.Start();
  churn_profiler.Start();
  churn_profiler.Stop();
  churn_profiler.Stop();
}

TEST_F(SamplingHeapChurnProfilerTest, RecordAllocFree) {
  ScopedSuppressRandomnessForTesting suppress;
  auto* sampler = PoissonAllocationSampler::Get();
  sampler->SetSamplingInterval(1024);

  auto* heap_profiler = SamplingHeapProfiler::Get();
  auto& churn_profiler = heap_profiler->churn_profiler();

  auto session = heap_profiler->Start(
      base::ByteSize(1024), SamplingHeapProfiler::Priority::kBackground);
  churn_profiler.Start();
  churn_profiler.SetSubsamplingChance(1.0);  // 100% chance for testing.

  void* const kAddress = reinterpret_cast<void*>(0x1234);
  sampler->OnAllocation(AllocationNotificationData(
      kAddress, 10000, nullptr, AllocationSubsystem::kManualForTesting));
  sampler->OnFree(
      FreeNotificationData(kAddress, AllocationSubsystem::kManualForTesting));

  std::vector<SamplingHeapProfiler::Sample> samples =
      churn_profiler.TakeSamples();
  EXPECT_EQ(1u, samples.size());
  if (!samples.empty()) {
    EXPECT_EQ(10000u, samples[0].size);
    EXPECT_EQ(AllocationSubsystem::kManualForTesting, samples[0].allocator);
  }

  // TakeSamples() should clear accumulated churn samples.
  EXPECT_TRUE(churn_profiler.TakeSamples().empty());

  heap_profiler->Stop(*session);
  churn_profiler.Stop();
}

TEST_F(SamplingHeapChurnProfilerTest, TakeSamples) {
  ScopedSuppressRandomnessForTesting suppress;
  auto* sampler = PoissonAllocationSampler::Get();
  sampler->SetSamplingInterval(1024);

  auto* heap_profiler = SamplingHeapProfiler::Get();
  auto& churn_profiler = heap_profiler->churn_profiler();

  auto session = heap_profiler->Start(
      base::ByteSize(1024), SamplingHeapProfiler::Priority::kBackground);
  churn_profiler.Start();
  churn_profiler.SetSubsamplingChance(1.0);

  void* const kAddress1 = reinterpret_cast<void*>(0x1111);
  sampler->OnAllocation(AllocationNotificationData(
      kAddress1, 1024, nullptr, AllocationSubsystem::kManualForTesting));
  sampler->OnFree(
      FreeNotificationData(kAddress1, AllocationSubsystem::kManualForTesting));

  std::vector<SamplingHeapProfiler::Sample> samples =
      churn_profiler.TakeSamples();
  ASSERT_EQ(1u, samples.size());
  EXPECT_EQ(1024u, samples[0].size);

  EXPECT_TRUE(churn_profiler.TakeSamples().empty());

  heap_profiler->Stop(*session);
  churn_profiler.Stop();
}

}  // namespace base
