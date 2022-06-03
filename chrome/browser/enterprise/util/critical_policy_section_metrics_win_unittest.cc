// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/util/critical_policy_section_metrics_win.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome {
namespace enterprise_util {

TEST(MeasureAndReportCriticalPolicySectionAcquisition, Do) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  MeasureAndReportCriticalPolicySectionAcquisition();
  task_environment.RunUntilIdle();

  base::HistogramBase::Count error_count = 0;

  // The total time should have been recorded once.
  histogram_tester.ExpectTotalCount(
      "Enterprise.EnterCriticalPolicySectionDelay.Total", 1);

  // The user lock should have been acquired or not exactly once.
  auto failed_count =
      histogram_tester
          .GetAllSamples(
              "Enterprise.EnterCriticalPolicySectionDelay.User.Failed")
          .size();
  if (failed_count) {
    EXPECT_EQ(failed_count, 1U) << "User.Failed";
    error_count += failed_count;
  } else {
    histogram_tester.ExpectTotalCount(
        "Enterprise.EnterCriticalPolicySectionDelay.User.Succeeded", 1);
  }

  // The machine lock should have been acquired or not exactly once.
  failed_count =
      histogram_tester
          .GetAllSamples(
              "Enterprise.EnterCriticalPolicySectionDelay.Machine.Failed")
          .size();
  if (failed_count) {
    EXPECT_EQ(failed_count, 1U) << "Machine.Failed";
    error_count += failed_count;
  } else {
    histogram_tester.ExpectTotalCount(
        "Enterprise.EnterCriticalPolicySectionDelay.Machine.Succeeded", 1);
  }

  // An error code should have been reported once for each failure.
  histogram_tester.ExpectTotalCount(
      "Enterprise.EnterCriticalPolicySectionError", error_count);
}

}  // namespace enterprise_util
}  // namespace chrome
