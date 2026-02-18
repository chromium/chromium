// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/available_physical_memory_monitor.h"

#include "base/byte_size.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestAvailablePhysicalMemoryMonitor
    : public AvailablePhysicalMemoryMonitor {
 public:
  TestAvailablePhysicalMemoryMonitor() = default;
  ~TestAvailablePhysicalMemoryMonitor() override = default;

  void SetFakeAvailableBytes(std::optional<base::ByteSize> bytes) {
    fake_bytes_ = bytes;
  }

  // Allow the test to manually trigger a poll.
  using AvailablePhysicalMemoryMonitor::OnMemoryCheckTimer;

 protected:
  std::optional<base::ByteSize> ComputeAvailableMemory() override {
    return fake_bytes_;
  }

 private:
  std::optional<base::ByteSize> fake_bytes_ = MiBU(100);
};

class TestObserver : public AvailablePhysicalMemoryMonitor::Observer {
 public:
  void OnAvailableMemoryUpdated(
      const AvailablePhysicalMemoryMonitor::MemorySample& sample) override {
    last_sample_ = sample;
    sample_count_++;
  }

  const std::optional<AvailablePhysicalMemoryMonitor::MemorySample>&
  last_sample() const {
    return last_sample_;
  }

  int sample_count() const { return sample_count_; }

 private:
  std::optional<AvailablePhysicalMemoryMonitor::MemorySample> last_sample_;
  int sample_count_ = 0;
};

}  // namespace

class AvailablePhysicalMemoryMonitorTest : public ::testing::Test {
 protected:
  // Use MOCK_TIME to control the timer.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that adding an observer starts the timer but does NOT trigger an
// immediate callback (safety for ScopedObservation).
TEST_F(AvailablePhysicalMemoryMonitorTest, NoImmediateCallbackOnAdd) {
  TestAvailablePhysicalMemoryMonitor monitor;
  TestObserver observer;

  monitor.SetFakeAvailableBytes(MiBU(250));
  monitor.AddObserver(&observer);

  // Should NOT have received a callback yet.
  EXPECT_EQ(observer.sample_count(), 0);
  EXPECT_FALSE(monitor.GetLastSample().has_value());

  monitor.RemoveObserver(&observer);
}

// Tests that GetLastSample returns data if available.
TEST_F(AvailablePhysicalMemoryMonitorTest, GetLastSample) {
  TestAvailablePhysicalMemoryMonitor monitor;
  monitor.SetFakeAvailableBytes(MiBU(100));

  TestObserver observer;
  monitor.AddObserver(&observer);

  // Initially empty because timer hasn't fired.
  EXPECT_FALSE(monitor.GetLastSample().has_value());

  // Force a check (simulating timer fire).
  monitor.OnMemoryCheckTimer();

  ASSERT_TRUE(monitor.GetLastSample().has_value());
  EXPECT_EQ(monitor.GetLastSample()->available_bytes, MiBU(100));
}

// Tests that the monitor polls repeatedly on the timer interval.
TEST_F(AvailablePhysicalMemoryMonitorTest, PollsPeriodically) {
  TestAvailablePhysicalMemoryMonitor monitor;
  TestObserver observer;

  monitor.AddObserver(&observer);
  EXPECT_EQ(observer.sample_count(), 0);

  // Fast forward by the poll interval (2 seconds).
  // This triggers the first timer firing.
  task_environment_.FastForwardBy(base::Seconds(2));

  EXPECT_EQ(observer.sample_count(), 1);

  // Change the value.
  monitor.SetFakeAvailableBytes(MiBU(300));

  // Advance time again.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_EQ(observer.sample_count(), 2);
  EXPECT_EQ(observer.last_sample()->available_bytes, MiBU(300));

  monitor.RemoveObserver(&observer);
}

// Tests that polling stops when the last observer is removed.
TEST_F(AvailablePhysicalMemoryMonitorTest, StopsPollingWhenNoObservers) {
  TestAvailablePhysicalMemoryMonitor monitor;
  TestObserver observer;

  monitor.AddObserver(&observer);

  // Trigger one sample.
  task_environment_.FastForwardBy(base::Seconds(2));
  int initial_count = observer.sample_count();
  EXPECT_EQ(initial_count, 1);

  monitor.RemoveObserver(&observer);

  // Advance time significantly.
  task_environment_.FastForwardBy(base::Seconds(10));

  // Count should not have increased.
  EXPECT_EQ(observer.sample_count(), initial_count);
}

TEST_F(AvailablePhysicalMemoryMonitorTest, LateObserverCanAccessCachedData) {
  TestAvailablePhysicalMemoryMonitor monitor;
  TestObserver observer1;
  TestObserver observer2;

  monitor.SetFakeAvailableBytes(MiBU(100));
  monitor.AddObserver(&observer1);

  // Fire timer to populate cache.
  task_environment_.FastForwardBy(base::Seconds(2));
  EXPECT_EQ(observer1.sample_count(), 1);

  // Add second observer. It should NOT get a callback.
  monitor.AddObserver(&observer2);
  EXPECT_EQ(observer2.sample_count(), 0);

  // But it SHOULD be able to read the cached value immediately.
  auto sample = monitor.GetLastSample();
  ASSERT_TRUE(sample.has_value());
  EXPECT_EQ(sample->available_bytes, MiBU(100));
}

}  // namespace base
