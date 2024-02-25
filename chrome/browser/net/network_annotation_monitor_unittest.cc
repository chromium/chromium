// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/network_annotation_monitor.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(NetworkAnnotationMonitorTest, ReportTest) {
  constexpr int32_t kTestDisabledHashCode = 123;
  constexpr int32_t kTestAllowedHashCode = 456;
  base::test::SingleThreadTaskEnvironment task_environment;
  base::HistogramTester histogram_tester;

  NetworkAnnotationMonitor monitor;
  monitor.SetDisabledAnnotationsForTesting({kTestDisabledHashCode});
  mojo::Remote<network::mojom::NetworkAnnotationMonitor> remote;
  remote.Bind(monitor.GetClient());

  remote->Report(kTestDisabledHashCode);
  remote->Report(kTestAllowedHashCode);
  monitor.FlushForTesting();

  // Disabled hash codes should trigger a violation.
  histogram_tester.ExpectBucketCount("NetworkAnnotationMonitor.PolicyViolation",
                                     kTestDisabledHashCode, 1);
  // Other hash codes should not trigger a violation.
  histogram_tester.ExpectBucketCount("NetworkAnnotationMonitor.PolicyViolation",
                                     kTestAllowedHashCode, 0);
}

// Verify that GetClient() can be called multiple times. This simulates what
// happens when the Network Service crashes and restarts.
TEST(NetworkAnnotationMonitorTest, GetClientResetTest) {
  base::test::SingleThreadTaskEnvironment task_environment;
  NetworkAnnotationMonitor monitor;

  EXPECT_TRUE(monitor.GetClient().is_valid());
  EXPECT_TRUE(monitor.GetClient().is_valid());
}
