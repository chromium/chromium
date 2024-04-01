// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/metrics.h"

#include <Security/Security.h>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mac_metrics {

TEST(MacMetricsTest, RecordAppFileSystemType) {
  base::HistogramTester histogram_tester;
  Metrics metrics;
  metrics.RecordAppFileSystemType();
  histogram_tester.ExpectTotalCount("Mac.AppFileSystemType", 1);
}

TEST(MacMetricsTest, RecordAppUpgradeCodeSignatureValidationStatus) {
  base::test::TaskEnvironment task_environment;
  base::HistogramTester histogram_tester;
  Metrics metrics;
  base::RunLoop run_loop;
  metrics.RecordAppUpgradeCodeSignatureValidation(
      base::OnceClosure(run_loop.QuitClosure()));
  run_loop.Run();
  histogram_tester.ExpectTotalCount(
      "Mac.AppUpgradeCodeSignatureValidationStatus", 1);
}

}  // namespace mac_metrics
