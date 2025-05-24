// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_metrics.h"

#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/app_list_public_test_util.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/views/app_list_bubble_view.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/capture_mode/capture_mode_test_util.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/capture_mode/capture_mode_api.h"
#include "ash/public/cpp/scanner/scanner_delegate.h"
#include "ash/scanner/fake_scanner_profile_scoped_delegate.h"
#include "ash/scanner/scanner_controller.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/specialized_features/feature_access_checker.h"
#include "scanner_metrics.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"

namespace ash {

namespace {

using enum ScannerFeatureUserState;
using ::testing::Return;

class ScannerMetricsParameterisedTest
    : public AshTestBase,
      public testing::WithParamInterface<ScannerFeatureUserState> {};

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    ScannerMetricsParameterisedTest,
    testing::ValuesIn<ScannerFeatureUserState>({
        kConsentDisclaimerAccepted,
        kConsentDisclaimerRejected,
        kSunfishScreenEnteredViaShortcut,
        kDeprecatedSunfishScreenInitialScreenCaptureSentToScannerServer,
        kScreenCaptureModeScannerButtonShown,
        kDeprecatedScreenCaptureModeInitialScreenCaptureSentToScannerServer,
        kNoActionsDetected,
        kNewCalendarEventActionDetected,
        kNewCalendarEventActionFinishedSuccessfully,
        kNewCalendarEventActionPopulationFailed,
        kNewContactActionDetected,
        kNewContactActionFinishedSuccessfully,
        kNewContactActionPopulationFailed,
        kNewGoogleSheetActionDetected,
        kNewGoogleSheetActionFinishedSuccessfully,
        kNewGoogleSheetActionPopulationFailed,
        kNewGoogleDocActionDetected,
        kNewGoogleDocActionFinishedSuccessfully,
        kNewGoogleDocActionPopulationFailed,
        kCopyToClipboardActionDetected,
        kCopyToClipboardActionFinishedSuccessfully,
        kCopyToClipboardActionPopulationFailed,
        kNewCalendarEventPopulatedActionExecutionFailed,
        kNewContactPopulatedActionExecutionFailed,
        kNewGoogleSheetPopulatedActionExecutionFailed,
        kNewGoogleDocPopulatedActionExecutionFailed,
        kCopyToClipboardPopulatedActionExecutionFailed,
        kCanShowUiReturnedFalse,
        kCanShowUiReturnedTrueWithoutConsent,
        kCanShowUiReturnedTrueWithConsent,
        kCanShowUiReturnedFalseDueToNoShellInstance,
        kCanShowUiReturnedFalseDueToNoControllerOnShell,
        kCanShowUiReturnedFalseDueToEnterprisePolicy,
        kCanShowUiReturnedFalseDueToNoProfileScopedDelegate,
        kCanShowUiReturnedFalseDueToSettingsToggle,
        kCanShowUiReturnedFalseDueToFeatureFlag,
        kCanShowUiReturnedFalseDueToFeatureManagement,
        kCanShowUiReturnedFalseDueToSecretKey,
        kCanShowUiReturnedFalseDueToAccountCapabilities,
        kCanShowUiReturnedFalseDueToCountry,
        kCanShowUiReturnedFalseDueToKioskMode,
        kLauncherShownWithoutSunfishSessionButton,
        kLauncherShownWithSunfishSessionButton,
        kSunfishSessionImageCapturedAndActionsNotFetched,
        kSunfishSessionImageCapturedAndActionsFetchStarted,
        kSmartActionsButtonImageCapturedAndActionsNotFetched,
        kSmartActionsButtonImageCapturedAndActionsFetchStarted,
        kSmartActionsButtonNotShownDueToFeatureChecks,
        kSmartActionsButtonNotShownDueToTextDetectionCancelled,
        kSmartActionsButtonNotShownDueToNoTextDetected,
        kSmartActionsButtonNotShownDueToCanShowUiFalse,
        kSmartActionsButtonNotShownDueToOffline,
    }));

TEST_P(ScannerMetricsParameterisedTest, Record) {
  base::HistogramTester histogram_tester;

  RecordScannerFeatureUserState(GetParam());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState", GetParam(),
                                     1);
}

TEST(ScannerMetricsNoFixtureTest, LauncherShownWithoutSunfishSessionButton) {
  base::HistogramTester histogram_tester;

  RecordSunfishSessionButtonVisibilityOnLauncherShown(/*is_visible=*/false);

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithoutSunfishSessionButton,
                                     1);
}

TEST(ScannerMetricsNoFixtureTest, LauncherShownWithSunfishSessionButton) {
  base::HistogramTester histogram_tester;

  RecordSunfishSessionButtonVisibilityOnLauncherShown(/*is_visible=*/true);

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithSunfishSessionButton, 1);
}

class ScannerMetricsTest : public AshTestBase {
 public:
  ScannerMetricsTest() {
    // Disable Sunfish so we can control `IsSunfishAllowedAndEnabled`.
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kScannerUpdate},
        /*disabled_features=*/{features::kSunfishFeature});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ScannerMetricsTest, ClamshellLauncherShownWithoutSunfishSessionButton) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings}));
  ASSERT_FALSE(CanShowSunfishOrScannerUi());

  // Open the app list by clicking on the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  AppListBubbleView* bubble_view = GetAppListBubbleView();
  ASSERT_TRUE(bubble_view);
  views::ImageButton* sunfish_button =
      bubble_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_FALSE(sunfish_button->GetVisible());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithoutSunfishSessionButton,
                                     1);
}

TEST_F(ScannerMetricsTest, ClamshellLauncherShownWithSunfishSessionButton) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  ASSERT_TRUE(CanShowSunfishOrScannerUi());

  // Open the app list by clicking on the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  AppListBubbleView* bubble_view = GetAppListBubbleView();
  ASSERT_TRUE(bubble_view);
  views::ImageButton* sunfish_button =
      bubble_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_TRUE(sunfish_button->GetVisible());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithSunfishSessionButton, 1);
}

TEST_F(ScannerMetricsTest, ClamshellLauncherButtonClicked) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  ASSERT_TRUE(CanShowSunfishOrScannerUi());

  // Open the app list by clicking on the home button.
  LeftClickOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  AppListBubbleView* bubble_view = GetAppListBubbleView();
  ASSERT_TRUE(bubble_view);
  views::ImageButton* sunfish_button =
      bubble_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_TRUE(sunfish_button->GetVisible());
  LeftClickOn(sunfish_button);

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kSunfishSessionStartedFromLauncherButton,
                                     1);
}

TEST_F(ScannerMetricsTest, TabletLauncherShownWithoutSunfishSessionButton) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{
          specialized_features::FeatureAccessFailure::kDisabledInSettings}));
  ASSERT_FALSE(CanShowSunfishOrScannerUi());

  // The app list should be open by default when we enter tablet mode.
  SwitchToTabletMode();
  AppListView* app_list_view = GetAppListView();
  ASSERT_TRUE(app_list_view);
  views::ImageButton* sunfish_button =
      app_list_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_FALSE(sunfish_button->GetVisible());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithoutSunfishSessionButton,
                                     1);
}

TEST_F(ScannerMetricsTest, TabletLauncherShownWithSunfishSessionButton) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  ASSERT_TRUE(CanShowSunfishOrScannerUi());

  // The app list should be open by default when we enter tablet mode.
  SwitchToTabletMode();
  AppListView* app_list_view = GetAppListView();
  ASSERT_TRUE(app_list_view);
  views::ImageButton* sunfish_button =
      app_list_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_TRUE(sunfish_button->GetVisible());

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kLauncherShownWithSunfishSessionButton, 1);
}

TEST_F(ScannerMetricsTest, TabletLauncherButtonClicked) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  ASSERT_TRUE(CanShowSunfishOrScannerUi());

  // The app list should be open by default when we enter tablet mode.
  SwitchToTabletMode();
  AppListView* app_list_view = GetAppListView();
  ASSERT_TRUE(app_list_view);
  views::ImageButton* sunfish_button =
      app_list_view->search_box_view()->sunfish_button();
  ASSERT_TRUE(sunfish_button);
  ASSERT_TRUE(sunfish_button->GetVisible());
  LeftClickOn(sunfish_button);

  histogram_tester.ExpectBucketCount("Ash.ScannerFeature.UserState",
                                     kSunfishSessionStartedFromLauncherButton,
                                     1);
}

TEST_F(ScannerMetricsTest, HomeButtonLongPress) {
  base::HistogramTester histogram_tester;
  ScannerController* scanner_controller = Shell::Get()->scanner_controller();
  ASSERT_TRUE(scanner_controller);
  auto* delegate = static_cast<FakeScannerProfileScopedDelegate*>(
      scanner_controller->delegate_for_testing()->GetProfileScopedDelegate());
  ON_CALL(*delegate, CheckFeatureAccess)
      .WillByDefault(Return(specialized_features::FeatureAccessFailureSet{}));
  ASSERT_TRUE(CanShowSunfishOrScannerUi());

  HomeButton* home_button =
      GetPrimaryShelf()->shelf_widget()->navigation_widget()->GetHomeButton();
  ui::GestureEvent long_press(
      0, 0, ui::EF_NONE, base::TimeTicks(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  home_button->OnGestureEvent(&long_press);

  histogram_tester.ExpectBucketCount(
      "Ash.ScannerFeature.UserState",
      kSunfishSessionStartedFromHomeButtonLongPress, 1);
}

}  // namespace

}  // namespace ash
