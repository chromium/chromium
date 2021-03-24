// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/apps/metrics/intent_handling_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/arc/intent_helper/arc_external_protocol_dialog.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

typedef testing::Test IntentHandlingMetricsTest;

TEST_F(IntentHandlingMetricsTest, TestRecordIntentPickerMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordIntentPickerMetrics(
      Source::kExternalProtocol, false,
      IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED,
      IntentHandlingMetrics::Platform::ARC);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog",
      IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);

  test.RecordIntentPickerMetrics(
      Source::kHttpOrHttps, true,
      IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED,
      IntentHandlingMetrics::Platform::ARC);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.IntentPickerAction",
      IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.IntentPickerDestinationPlatform",
      IntentHandlingMetrics::Platform::ARC, 1);
}

TEST_F(IntentHandlingMetricsTest,
       TestRecordIntentPickerUserInteractionMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordIntentPickerUserInteractionMetrics(
      "app_package", PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
      Source::kExternalProtocol, true);

  histogram_tester.ExpectBucketCount(
      "Arc.UserInteraction", arc::UserInteractionType::APP_STARTED_FROM_LINK,
      1);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog",
      IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED, 1);
}

TEST_F(IntentHandlingMetricsTest, TestRecordExternalProtocolMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test = IntentHandlingMetrics();
  test.RecordExternalProtocolMetrics(arc::Scheme::IRC, PickerEntryType::kArc,
                                     /*accepted=*/true, /*persisted=*/true);

  histogram_tester.ExpectBucketCount(
      "ChromeOS.Apps.ExternalProtocolDialog.Accepted",
      arc::ProtocolAction::IRC_ACCEPTED_PERSISTED, 1);
}

TEST_F(IntentHandlingMetricsTest, TestRecordOpenBrowserMetrics) {
  base::HistogramTester histogram_tester;

  IntentHandlingMetrics test;
  test.RecordOpenBrowserMetrics(IntentHandlingMetrics::AppType::kArc);

  histogram_tester.ExpectBucketCount("ChromeOS.Apps.OpenBrowser",
                                     IntentHandlingMetrics::AppType::kArc, 1);
}

TEST(IntentHandlingMetricsTest, TestGetPickerAction) {
  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ERROR_BEFORE_PICKER,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::ERROR_BEFORE_PICKER,
                /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::ERROR_BEFORE_PICKER,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::ERROR_BEFORE_PICKER,
          /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ERROR_BEFORE_PICKER,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::ERROR_BEFORE_PICKER,
                /*should_persist=*/false));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::ERROR_BEFORE_PICKER,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::ERROR_BEFORE_PICKER,
          /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ERROR_AFTER_PICKER,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::ERROR_AFTER_PICKER,
                /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::ERROR_AFTER_PICKER,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::ERROR_AFTER_PICKER,
          /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ERROR_AFTER_PICKER,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::ERROR_AFTER_PICKER,
                /*should_persist=*/false));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::ERROR_AFTER_PICKER,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::ERROR_AFTER_PICKER,
          /*should_persist=*/false));

  // Expect PickerAction::DIALOG_DEACTIVATED if the close_reason is
  // DIALOG_DEACTIVATED.
  EXPECT_EQ(IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::DIALOG_DEACTIVATED,
          /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::DIALOG_DEACTIVATED,
                /*should_persist=*/false));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::DIALOG_DEACTIVATED,
          /*should_persist=*/false));

  // Expect PREFERRED FOUND depending on entry type if the close_reason is
  // PREFERRED_APP_FOUND.
  EXPECT_EQ(IntentHandlingMetrics::PickerAction::PREFERRED_CHROME_BROWSER_FOUND,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::PREFERRED_ARC_ACTIVITY_FOUND,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::PREFERRED_APP_FOUND,
          /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::PREFERRED_PWA_FOUND,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kWeb, IntentPickerCloseReason::PREFERRED_APP_FOUND,
          /*should_persist=*/true));

  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kDevice, IntentPickerCloseReason::PREFERRED_APP_FOUND,
      /*should_persist=*/true));

  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kMacOs, IntentPickerCloseReason::PREFERRED_APP_FOUND,
      /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::PREFERRED_CHROME_BROWSER_FOUND,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kUnknown,
                IntentPickerCloseReason::PREFERRED_APP_FOUND,
                /*should_persist=*/false));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::PREFERRED_ARC_ACTIVITY_FOUND,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kArc, IntentPickerCloseReason::PREFERRED_APP_FOUND,
          /*should_persist=*/false));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::PREFERRED_PWA_FOUND,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kWeb, IntentPickerCloseReason::PREFERRED_APP_FOUND,
          /*should_persist=*/false));

  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kDevice, IntentPickerCloseReason::PREFERRED_APP_FOUND,
      /*should_persist=*/false));

  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kMacOs, IntentPickerCloseReason::PREFERRED_APP_FOUND,
      /*should_persist=*/false));

  // Expect PREFERRED depending on the value of |should_persist|, and
  // |entry_type| to be ignored if reason is STAY_IN_CHROME.
  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::CHROME_PREFERRED_PRESSED,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kUnknown, IntentPickerCloseReason::STAY_IN_CHROME,
          /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::CHROME_PREFERRED_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kArc, IntentPickerCloseReason::STAY_IN_CHROME,
                /*should_persist=*/true));

  EXPECT_EQ(
      IntentHandlingMetrics::PickerAction::CHROME_PRESSED,
      IntentHandlingMetrics::GetPickerAction(
          PickerEntryType::kUnknown, IntentPickerCloseReason::STAY_IN_CHROME,
          /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::CHROME_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kArc, IntentPickerCloseReason::STAY_IN_CHROME,
                /*should_persist=*/false));

  // Expect PREFERRED depending on the value of |should_persist|, and
  // different entry type to be chosen if reason is OPEN_APP.
  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kUnknown, IntentPickerCloseReason::OPEN_APP,
      /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::PWA_APP_PREFERRED_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kWeb, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::DEVICE_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kDevice, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/true));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::MAC_OS_APP_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kMacOs, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/true));

  EXPECT_DCHECK_DEATH(IntentHandlingMetrics::GetPickerAction(
      PickerEntryType::kUnknown, IntentPickerCloseReason::OPEN_APP,
      /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::ARC_APP_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kArc, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::PWA_APP_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kWeb, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::DEVICE_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kDevice, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/false));

  EXPECT_EQ(IntentHandlingMetrics::PickerAction::MAC_OS_APP_PRESSED,
            IntentHandlingMetrics::GetPickerAction(
                PickerEntryType::kMacOs, IntentPickerCloseReason::OPEN_APP,
                /*should_persist=*/false));
}

TEST(IntentHandlingMetricsTest, TestGetDestinationPlatform) {
  const std::string app_id = "fake_package";

  // When the PickerAction is either ERROR or DIALOG_DEACTIVATED we MUST stay
  // in Chrome not taking into account the selected_app_package.
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::CHROME,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id, IntentHandlingMetrics::PickerAction::ERROR_BEFORE_PICKER));
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::CHROME,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id, IntentHandlingMetrics::PickerAction::ERROR_AFTER_PICKER));
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::CHROME,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id, IntentHandlingMetrics::PickerAction::DIALOG_DEACTIVATED));

  // When stay in chrome is selected, or when user remember the stay in
  // chrome selection, always expect the platform to be CHROME.
  EXPECT_EQ(IntentHandlingMetrics::Platform::CHROME,
            IntentHandlingMetrics::GetDestinationPlatform(
                app_id, IntentHandlingMetrics::PickerAction::CHROME_PRESSED));
  EXPECT_EQ(IntentHandlingMetrics::Platform::CHROME,
            IntentHandlingMetrics::GetDestinationPlatform(
                app_id,
                IntentHandlingMetrics::PickerAction::CHROME_PREFERRED_PRESSED));
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::CHROME,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id,
          IntentHandlingMetrics::PickerAction::PREFERRED_CHROME_BROWSER_FOUND));

  // When user select PWA or PWA auto launched, always expect the platform to
  // be PWA.
  EXPECT_EQ(IntentHandlingMetrics::Platform::PWA,
            IntentHandlingMetrics::GetDestinationPlatform(
                app_id, IntentHandlingMetrics::PickerAction::PWA_APP_PRESSED));

  EXPECT_EQ(
      IntentHandlingMetrics::Platform::PWA,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id,
          IntentHandlingMetrics::PickerAction::PWA_APP_PREFERRED_PRESSED));
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::PWA,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id, IntentHandlingMetrics::PickerAction::PREFERRED_PWA_FOUND));

  // When user select ARC app or ARC app auto launched, always expect the
  // platform to be ARC.
  EXPECT_EQ(IntentHandlingMetrics::Platform::ARC,
            IntentHandlingMetrics::GetDestinationPlatform(
                app_id, IntentHandlingMetrics::PickerAction::ARC_APP_PRESSED));

  EXPECT_EQ(
      IntentHandlingMetrics::Platform::ARC,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id,
          IntentHandlingMetrics::PickerAction::ARC_APP_PREFERRED_PRESSED));
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::ARC,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id,
          IntentHandlingMetrics::PickerAction::PREFERRED_ARC_ACTIVITY_FOUND));

  // When user select Mac OS app, expect the platform to be MAC_OS.
  EXPECT_EQ(
      IntentHandlingMetrics::Platform::MAC_OS,
      IntentHandlingMetrics::GetDestinationPlatform(
          app_id, IntentHandlingMetrics::PickerAction::MAC_OS_APP_PRESSED));

  // When user select a device, expect the platform to be DEVICE.
  EXPECT_EQ(IntentHandlingMetrics::Platform::DEVICE,
            IntentHandlingMetrics::GetDestinationPlatform(
                app_id, IntentHandlingMetrics::PickerAction::DEVICE_PRESSED));
}

}  // namespace apps
