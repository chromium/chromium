// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/finds/core/finds_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace finds {

TEST(FindsMetricsTest, RecordNotificationInteraction) {
  base::HistogramTester histogram_tester;
  RecordNotificationInteraction(FindsNotificationUserInteraction::kClick);
  histogram_tester.ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationInteraction",
      FindsNotificationUserInteraction::kClick, 1);

  RecordNotificationInteraction(FindsNotificationUserInteraction::kDismiss);
  histogram_tester.ExpectBucketCount(
      "Notifications.ChromeFinds.NotificationInteraction",
      FindsNotificationUserInteraction::kDismiss, 1);

  RecordNotificationInteraction(
      FindsNotificationUserInteraction::kHelpfulButtonClick);
  histogram_tester.ExpectBucketCount(
      "Notifications.ChromeFinds.NotificationInteraction",
      FindsNotificationUserInteraction::kHelpfulButtonClick, 1);

  RecordNotificationInteraction(
      FindsNotificationUserInteraction::kUnhelpfulButtonClick);
  histogram_tester.ExpectBucketCount(
      "Notifications.ChromeFinds.NotificationInteraction",
      FindsNotificationUserInteraction::kUnhelpfulButtonClick, 1);
}

TEST(FindsMetricsTest, RecordNotificationShown) {
  base::HistogramTester histogram_tester;
  RecordNotificationShown();
  histogram_tester.ExpectUniqueSample(
      "Notifications.ChromeFinds.NotificationShown", true, 1);
}

}  // namespace finds
