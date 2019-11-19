// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"

#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_monitor {

class MetricEvaluatorsHelperWinTest : public testing::Test {
 public:
  MetricEvaluatorsHelperWinTest() = default;
  ~MetricEvaluatorsHelperWinTest() override = default;

 protected:
  base::test::TaskEnvironment task_environment_;
  MetricEvaluatorsHelperWin metric_evaluator_helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MetricEvaluatorsHelperWinTest);
};

TEST_F(MetricEvaluatorsHelperWinTest, GetFreeMemory) {
  PostTask(FROM_HERE, base::BindLambdaForTesting([&] {
             auto value = metric_evaluator_helper_.GetFreePhysicalMemoryMb();
             EXPECT_TRUE(value);
             EXPECT_GT(value.value(), 0);
           }));
  task_environment_.RunUntilIdle();
}

// TODO(https://crbug.com/956638): Investigate why the initialization of WMI
// might fail in some situations and reenable this test.
TEST_F(MetricEvaluatorsHelperWinTest, DISABLED_DiskIdleTime) {
  while (!metric_evaluator_helper_.wmi_refresher_initialized_for_testing())
    task_environment_.RunUntilIdle();

  // Measuring the disk idle time will always return base::nullopt for the first
  // sample on Windows.
  metric_evaluator_helper_.GetDiskIdleTimePercent();
  auto refreshed_metrics = metric_evaluator_helper_.GetDiskIdleTimePercent();
  EXPECT_TRUE(refreshed_metrics);
  EXPECT_LE(0.0, refreshed_metrics.value());
  EXPECT_GE(1.0, refreshed_metrics.value());
}

}  // namespace performance_monitor
