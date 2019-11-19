// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/upgrade_metrics_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "testing/gtest/include/gtest/gtest.h"

class UpgradeMetricsProviderTest : public testing::Test {
 public:
  UpgradeMetricsProviderTest() {}

  void TestHistogramLevel(
      UpgradeDetector::UpgradeNotificationAnnoyanceLevel level) {
    UpgradeDetector::GetInstance()->set_upgrade_notification_stage(level);
    base::HistogramTester histogram_tester;
    metrics_provider_.ProvideCurrentSessionData(nullptr);
    histogram_tester.ExpectUniqueSample("UpgradeDetector.NotificationStage",
                                        level, 1);
  }

 private:
  UpgradeMetricsProvider metrics_provider_;

  DISALLOW_COPY_AND_ASSIGN(UpgradeMetricsProviderTest);
};

TEST_F(UpgradeMetricsProviderTest, HistogramCheck) {
  base::test::TaskEnvironment task_environment;
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_NONE);
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_VERY_LOW);
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_LOW);
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_ELEVATED);
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_HIGH);
  TestHistogramLevel(UpgradeDetector::UPGRADE_ANNOYANCE_CRITICAL);
}
