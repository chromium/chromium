// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/usage_scenario/chrome_responsiveness_calculator_delegate.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/metrics/usage_scenario/usage_scenario_data_store.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeResponsivenessCalculatorDelegateTest : public testing::Test {
 public:
  ChromeResponsivenessCalculatorDelegateTest()
      : delegate_(ChromeResponsivenessCalculatorDelegate::CreateForTesting(
            &data_store_)) {}

  ~ChromeResponsivenessCalculatorDelegateTest() override = default;

 protected:
  base::HistogramTester histogram_tester_;

  UsageScenarioDataStoreImpl data_store_;
  std::unique_ptr<ChromeResponsivenessCalculatorDelegate> delegate_;
};

// Tests that the right usage scenario suffix is added. Usage scenario suffixes
// are already extensively tested in usage_scenario_unittest.cc so no need to
// test them all.
TEST_F(ChromeResponsivenessCalculatorDelegateTest, Navigation) {
  // Pretend a navigation happened in the current interval.
  data_store_.OnTabAdded();
  data_store_.OnWindowVisible();
  data_store_.OnTopLevelNavigation();

  // Emit a "10" sample.
  delegate_->OnMeasurementIntervalEnded();
  delegate_->OnResponsivenessEmitted(10, 0, 50, 50);

  histogram_tester_.ExpectUniqueSample(
      "Browser.MainThreadsCongestion.Navigation", 10, 1);
}

// Tests that `OnResponsivenessEmitted()` only consider the last interval by
// calling `OnMeasurementIntervalEnded()` twice.
TEST_F(ChromeResponsivenessCalculatorDelegateTest, OnMeasurementIntervalEnded) {
  // Pretend a navigation happened in the current interval.
  data_store_.OnTabAdded();
  data_store_.OnWindowVisible();
  data_store_.OnTopLevelNavigation();

  // Reset the current interval.
  delegate_->OnMeasurementIntervalEnded();

  // Emit a "10" sample.
  delegate_->OnMeasurementIntervalEnded();
  delegate_->OnResponsivenessEmitted(10, 0, 50, 50);

  // The "Navigation" suffix should take priority over the "Passive" suffix if
  // the interval data was not reset with the first call to
  // OnMeasurementIntervalEnded()
  histogram_tester_.ExpectUniqueSample("Browser.MainThreadsCongestion.Passive",
                                       10, 1);
}

TEST_F(ChromeResponsivenessCalculatorDelegateTest, UsedScenario) {
  // First interval. Not visible so there should not be any samples.
  delegate_->OnMeasurementIntervalEnded();
  delegate_->OnResponsivenessEmitted(10, 0, 50, 50);

  histogram_tester_.ExpectUniqueSample("Browser.MainThreadsCongestion.Used", 10,
                                       0);

  // Second interval. With a visible window so a sample will be emitted.
  data_store_.OnTabAdded();
  data_store_.OnWindowVisible();

  delegate_->OnMeasurementIntervalEnded();
  delegate_->OnResponsivenessEmitted(10, 0, 50, 50);

  histogram_tester_.ExpectUniqueSample("Browser.MainThreadsCongestion.Used", 10,
                                       1);
}
