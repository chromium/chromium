// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/available_memory_monitor.h"

#include <cstdint>
#include <optional>

#include "base/byte_size.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// Helper to cleanly construct cross-platform mock samples.
std::optional<AvailableMemoryMonitor::MemorySample> CreateSample(
    uint64_t phys_bytes
#if BUILDFLAG(IS_WIN)
    ,
    uint64_t avail_commit_bytes = 0,
    uint64_t total_commit_bytes = 0
#endif
) {
  AvailableMemoryMonitor::MemorySample sample;
  sample.timestamp = base::TimeTicks::Now();
  sample.available_physical_bytes = base::ByteSize(phys_bytes);

#if BUILDFLAG(IS_WIN)
  sample.available_commit_bytes = base::ByteSize(avail_commit_bytes);
  sample.total_commit_bytes = base::ByteSize(total_commit_bytes);
#endif

  return sample;
}

class TestAvailableMemoryMonitor : public AvailableMemoryMonitor {
 public:
  TestAvailableMemoryMonitor() = default;
  TestAvailableMemoryMonitor(const TestAvailableMemoryMonitor&) = delete;
  TestAvailableMemoryMonitor& operator=(const TestAvailableMemoryMonitor&) =
      delete;
  ~TestAvailableMemoryMonitor() override = default;

  void SetFakeSample(std::optional<MemorySample> sample) {
    fake_sample_ = sample;
  }

  // Allow the test to manually trigger a poll.
  using AvailableMemoryMonitor::OnMemoryCheckTimer;

 protected:
  std::optional<MemorySample> ComputeAvailableMemory() override {
    return fake_sample_;
  }

 private:
  // Initialize with a default valid state.
  std::optional<MemorySample> fake_sample_ = CreateSample(100);
};

class TestObserver : public AvailableMemoryMonitor::Observer {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  void OnAvailableMemoryUpdated(
      const AvailableMemoryMonitor::MemorySample& sample) override {
    last_sample_ = sample;
    ++sample_count_;
  }

  const std::optional<AvailableMemoryMonitor::MemorySample>& last_sample()
      const {
    return last_sample_;
  }

  int sample_count() const { return sample_count_; }

 private:
  std::optional<AvailableMemoryMonitor::MemorySample> last_sample_;
  int sample_count_ = 0;
};

}  // namespace

class AvailableMemoryMonitorTest : public ::testing::Test {
 protected:
  // Use MOCK_TIME to control the timer deterministically.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(AvailableMemoryMonitorTest, NoImmediateCallbackOnAdd) {
  TestAvailableMemoryMonitor monitor;
  TestObserver observer;

  monitor.SetFakeSample(CreateSample(250));
  monitor.AddObserver(&observer);

  // The observer should not have received a callback immediately upon addition.
  EXPECT_EQ(observer.sample_count(), 0);
  EXPECT_FALSE(monitor.GetLastSample().has_value());

  monitor.RemoveObserver(&observer);
}

TEST_F(AvailableMemoryMonitorTest, GetLastSample) {
  TestAvailableMemoryMonitor monitor;
  monitor.SetFakeSample(CreateSample(100));

  TestObserver observer;
  monitor.AddObserver(&observer);

  // The cache is initially empty because the timer has not fired.
  EXPECT_FALSE(monitor.GetLastSample().has_value());

  // Force a check to simulate the timer firing.
  monitor.OnMemoryCheckTimer();

  ASSERT_TRUE(monitor.GetLastSample().has_value());
  EXPECT_EQ(monitor.GetLastSample()->available_physical_bytes,
            base::ByteSize(100));
}

TEST_F(AvailableMemoryMonitorTest, PollsPeriodically) {
  TestAvailableMemoryMonitor monitor;
  TestObserver observer;

  monitor.AddObserver(&observer);
  EXPECT_EQ(observer.sample_count(), 0);

  // Fast forward by the poll interval (2 seconds) to trigger the first check.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_EQ(observer.sample_count(), 1);

  // Update the mock sample.
  monitor.SetFakeSample(CreateSample(300));

  // Advance time again to trigger the second check.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_EQ(observer.sample_count(), 2);
  EXPECT_EQ(observer.last_sample()->available_physical_bytes,
            base::ByteSize(300));

  monitor.RemoveObserver(&observer);
}

TEST_F(AvailableMemoryMonitorTest, StopsPollingWhenNoObservers) {
  TestAvailableMemoryMonitor monitor;
  TestObserver observer;

  monitor.SetFakeSample(CreateSample(100));
  monitor.AddObserver(&observer);

  // Trigger one sample update.
  task_environment_.FastForwardBy(base::Seconds(2));
  const int initial_count = observer.sample_count();
  EXPECT_EQ(initial_count, 1);

  monitor.RemoveObserver(&observer);

  // Advance time significantly after the observer is removed.
  task_environment_.FastForwardBy(base::Seconds(10));

  // The callback count should not have increased.
  EXPECT_EQ(observer.sample_count(), initial_count);
}

TEST_F(AvailableMemoryMonitorTest, LateObserverCanAccessCachedData) {
  TestAvailableMemoryMonitor monitor;
  TestObserver observer1;
  TestObserver observer2;

  monitor.SetFakeSample(CreateSample(100));
  monitor.AddObserver(&observer1);

  // Fire the timer to populate the cache.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_EQ(observer1.sample_count(), 1);

  // Add the second observer. It should not get a callback.
  monitor.AddObserver(&observer2);
  EXPECT_EQ(observer2.sample_count(), 0);

  // However, it should be able to read the cached value immediately.
  std::optional<AvailableMemoryMonitor::MemorySample> sample =
      monitor.GetLastSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->available_physical_bytes, base::ByteSize(100));

  monitor.RemoveObserver(&observer1);
  monitor.RemoveObserver(&observer2);
}

#if BUILDFLAG(IS_WIN)

TEST_F(AvailableMemoryMonitorTest, WindowsCommitBytesPlumbing) {
  TestAvailableMemoryMonitor monitor;
  // Create a mock sample with 100 physical, 200 available commit, 300 total
  // commit.
  monitor.SetFakeSample(CreateSample(100, 200, 300));

  TestObserver observer;
  monitor.AddObserver(&observer);

  // Force a check to simulate the timer firing.
  monitor.OnMemoryCheckTimer();

  // Verify the cache holds the correct Windows-specific commit bytes.
  ASSERT_TRUE(monitor.GetLastSample().has_value());
  EXPECT_EQ(monitor.GetLastSample()->available_commit_bytes,
            base::ByteSize(200));
  EXPECT_EQ(monitor.GetLastSample()->total_commit_bytes, base::ByteSize(300));

  // Verify the observer payload received the correct Windows-specific bytes.
  ASSERT_EQ(observer.sample_count(), 1);
  ASSERT_TRUE(observer.last_sample().has_value());
  EXPECT_EQ(observer.last_sample()->available_commit_bytes,
            base::ByteSize(200));
  EXPECT_EQ(observer.last_sample()->total_commit_bytes, base::ByteSize(300));
}

#endif  // BUILDFLAG(IS_WIN)

}  // namespace base
