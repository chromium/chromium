// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/memory_pressure_monitor_utils.h"

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace memory {
namespace {

constexpr base::TimeDelta kDefaultWindowLength =
    base::TimeDelta::FromMinutes(1);

}  // namespace

// Test class used to expose some protected members from
// internal::ObservationWindow and to simplify the tests by removing the
// template argument.
class TestObservationWindow : public internal::ObservationWindow<int> {
 public:
  TestObservationWindow()
      : internal::ObservationWindow<int>(kDefaultWindowLength) {}
  ~TestObservationWindow() override = default;

  using internal::ObservationWindow<int>::observations_for_testing;
  using internal::ObservationWindow<int>::set_clock_for_testing;

  MOCK_METHOD1(OnSampleAdded, void(const int& sample));
  MOCK_METHOD1(OnSampleRemoved, void(const int& sample));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObservationWindow);
};

class ObservationWindowTest : public testing::Test {
 public:
  ObservationWindowTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        tick_clock_(task_environment_.GetMockTickClock()) {}
  ~ObservationWindowTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;

  const base::TickClock* tick_clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ObservationWindowTest);
};

TEST_F(ObservationWindowTest, OnSample) {
  ::testing::StrictMock<TestObservationWindow> window;

  typedef decltype(window.observations_for_testing()) ObservationContainerType;

  window.set_clock_for_testing(tick_clock_);

  // The window is expected to be empty by default.
  EXPECT_TRUE(window.observations_for_testing().empty());

  // Add a first sample.
  int t0_sample = 1;
  base::TimeTicks t0_timestamp = tick_clock_->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t0_sample)).Times(1);
  window.OnSample(t0_sample);
  ::testing::Mock::VerifyAndClear(&window);

  // decltype is used here to avoid having to expose the internal datatype used
  // to represent this window.
  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t0_timestamp, t0_sample},
              }));

  // Fast forward by the length of the observation window, no sample should be
  // removed as all samples have an age that doesn't exceed the window length.
  task_environment_.FastForwardBy(kDefaultWindowLength);

  int t1_sample = 2;
  base::TimeTicks t1_timestamp = tick_clock_->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t1_sample)).Times(1);
  window.OnSample(t1_sample);
  ::testing::Mock::VerifyAndClear(&window);

  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t0_timestamp, t0_sample},
                  {t1_timestamp, t1_sample},
              }));

  // Fast forward by one second, the first sample should be removed the next
  // time a sample gets added as its age exceed the length of the observation
  // window.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));

  int t2_sample = 3;
  base::TimeTicks t2_timestamp = tick_clock_->NowTicks();

  EXPECT_CALL(window, OnSampleAdded(t2_sample)).Times(1);
  EXPECT_CALL(window, OnSampleRemoved(t0_sample)).Times(1);
  window.OnSample(t2_sample);
  ::testing::Mock::VerifyAndClear(&window);

  EXPECT_THAT(window.observations_for_testing(),
              ::testing::ContainerEq(ObservationContainerType{
                  {t1_timestamp, t1_sample},
                  {t2_timestamp, t2_sample},
              }));
}

TEST_F(ObservationWindowTest, FreeMemoryObservationWindow) {
  FreeMemoryObservationWindow::Config window_config = {};
  FreeMemoryObservationWindow window(kDefaultWindowLength, window_config);
  window.set_clock_for_testing(tick_clock_);

  DCHECK_GT(window_config.low_memory_early_limit_mb,
            window_config.low_memory_critical_limit_mb);

  // Fill the window with samples with a value higher than the higher limit.
  for (size_t i = 0; i < window_config.min_sample_count; ++i) {
    EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
    EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());
    window.OnSample(window_config.low_memory_early_limit_mb + 1);
  }

  // The window has enough samples to be evaluated but they're all above the
  // thresholds.
  EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
  EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());

  // Test the detection that the system has reached the early limit.

  // Remove all the observations from the window.
  task_environment_.FastForwardBy(kDefaultWindowLength +
                                  base::TimeDelta::FromSeconds(1));

  const size_t min_sample_count_to_be_positive =
      static_cast<size_t>(window_config.sample_ratio_to_be_positive *
                          window_config.min_sample_count);

  DCHECK_GE(window_config.min_sample_count, min_sample_count_to_be_positive);

  for (size_t i = 0;
       i < window_config.min_sample_count - min_sample_count_to_be_positive;
       ++i) {
    window.OnSample(window_config.low_memory_early_limit_mb + 1);
    EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
    EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());
  }

  for (size_t i = 0; i < min_sample_count_to_be_positive; ++i) {
    EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
    EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());
    window.OnSample(window_config.low_memory_early_limit_mb - 1);
  }
  EXPECT_TRUE(window.MemoryIsUnderEarlyLimit());
  EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());

  // Test the detection that the system has reached the critical limit.

  // Remove all the observations from the window.
  task_environment_.FastForwardBy(kDefaultWindowLength +
                                  base::TimeDelta::FromSeconds(1));

  for (size_t i = 0;
       i < window_config.min_sample_count - min_sample_count_to_be_positive;
       ++i) {
    window.OnSample(window_config.low_memory_critical_limit_mb + 1);
    EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
    EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());
  }

  for (size_t i = 0; i < min_sample_count_to_be_positive; ++i) {
    EXPECT_FALSE(window.MemoryIsUnderEarlyLimit());
    EXPECT_FALSE(window.MemoryIsUnderCriticalLimit());
    window.OnSample(window_config.low_memory_critical_limit_mb - 1);
  }
  EXPECT_TRUE(window.MemoryIsUnderEarlyLimit());
  EXPECT_TRUE(window.MemoryIsUnderCriticalLimit());
}

TEST_F(ObservationWindowTest, DiskIdleTimeObservationWindow) {
  const float kDiskIdleTimeThreshold = 0.5;

  DiskIdleTimeObservationWindow window(kDefaultWindowLength,
                                       kDiskIdleTimeThreshold);
  window.set_clock_for_testing(tick_clock_);

  window.OnSample(1.0);

  // The average disk idle time is equal to 1, it's not under the threshold.
  EXPECT_FALSE(window.DiskIdleTimeIsLow());

  window.OnSample(0.1);
  // The average disk idle time is equal to (1 + 0.1) / 2 = 0.55, it's still not
  // under the threshold.
  EXPECT_FALSE(window.DiskIdleTimeIsLow());

  window.OnSample(0.1);
  // The average disk idle time is now equal to (1 + 0.1 + 0.1) / 3 = 0.4, it's
  // now under the threshold.
  EXPECT_TRUE(window.DiskIdleTimeIsLow());

  // Remove all the observations from the window.
  task_environment_.FastForwardBy(kDefaultWindowLength +
                                  base::TimeDelta::FromSeconds(1));

  // Add a sample under the threshold, the disk idle should be considered as low
  // as it's under the threshold.
  window.OnSample(0.1);
  EXPECT_TRUE(window.DiskIdleTimeIsLow());

  window.OnSample(1.0);
  // The average disk idle time is equal to (1 + 0.1) / 2 = 0.55, it's not under
  // the threshold anymore.
  EXPECT_FALSE(window.DiskIdleTimeIsLow());
}

}  // namespace memory
