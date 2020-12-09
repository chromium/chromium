// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/metrics_reporting_observer.h"

#include "base/test/task_environment.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/metrics/metrics_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

// MetricsReportingObserver that avoids calling ChangeMetricsReportingState().
class TestMetricsReportingObserver : public MetricsReportingObserver {
 public:
  explicit TestMetricsReportingObserver(PrefService* local_state)
      : MetricsReportingObserver(local_state) {}
  TestMetricsReportingObserver(const TestMetricsReportingObserver&) = delete;
  TestMetricsReportingObserver& operator=(const TestMetricsReportingObserver&) =
      delete;
  ~TestMetricsReportingObserver() override = default;

  // MetricsReportingObserver:
  void DoChangeMetricsReportingState(bool enabled) override {
    metrics_reporting_enabled_ = enabled;
    ++change_metrics_reporting_count_;
  }

  base::Optional<bool> metrics_reporting_enabled_;
  int change_metrics_reporting_count_ = 0;
};

TEST(MetricsReportingObserverTest, ChangesOnlyApplyOnce) {
  base::test::TaskEnvironment task_environment;

  // Simulate starting with metrics reporting starting enabled.
  ScopedTestingLocalState local_state(TestingBrowserProcess::GetGlobal());
  local_state.Get()->SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  // Construct object under test.
  TestMetricsReportingObserver observer(local_state.Get());

  // Receiving metrics reporting enabled from ash does not call
  // ChangeMetricsReportingState() because metrics are already enabled.
  observer.OnMetricsReportingChanged(true);
  EXPECT_FALSE(observer.metrics_reporting_enabled_.has_value());
  EXPECT_EQ(0, observer.change_metrics_reporting_count_);

  // However, receiving metrics reporting disabled from ash does call
  // ChangeMetricsReportingState() because the value changed.
  observer.OnMetricsReportingChanged(false);
  EXPECT_TRUE(observer.metrics_reporting_enabled_.has_value());
  EXPECT_FALSE(observer.metrics_reporting_enabled_.value());
  EXPECT_EQ(1, observer.change_metrics_reporting_count_);
}
