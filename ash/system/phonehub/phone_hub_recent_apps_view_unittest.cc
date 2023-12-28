// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_apps_view.h"

#include "ash/constants/ash_features.h"
#include "ash/system/phonehub/phone_connected_view.h"
#include "ash/system/phonehub/phone_hub_recent_app_button.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/fake_recent_apps_interaction_handler.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"
#include "ui/events/test/test_event.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/button_test_api.h"

namespace ash {

namespace {

using FeatureState = multidevice_setup::mojom::FeatureState;
using RecentAppsUiState =
    phonehub::RecentAppsInteractionHandler::RecentAppsUiState;
using RecentAppsViewUiState = phone_hub_metrics::RecentAppsViewUiState;

constexpr char kRecentAppsStateOnBubbleOpenedHistogramName[] =
    "PhoneHub.RecentApps.State.OnBubbleOpened";
constexpr char kRecentAppsTransitionToFailedLatencyHistogramName[] =
    "PhoneHub.RecentApps.TransitionToFailed.Latency";
constexpr char kRecentAppsTransitionToSuccessLatencyHistogramName[] =
    "PhoneHub.RecentApps.TransitionToSuccess.Latency";

const char16_t kAppName[] = u"Test App";
const char kPackageName[] = "com.google.testapp";
const int64_t kUserId = 0;

}  // namespace

class RecentAppButtonsViewTest : public AshTestBase {
 public:
  RecentAppButtonsViewTest() = default;
  ~RecentAppButtonsViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                              features::kEcheLauncherIconsInMoreAppsButton,
                              features::kEcheNetworkConnectionState},
        /*disabled_features=*/{});

    phone_hub_recent_apps_view_ = std::make_unique<PhoneHubRecentAppsView>(
        &fake_recent_apps_interaction_handler_, &fake_phone_hub_manager_,
        connected_view_);
  }

  void TearDown() override {
    phone_hub_recent_apps_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  PhoneHubRecentAppsView* recent_apps_view() {
    return phone_hub_recent_apps_view_.get();
  }

  void NotifyRecentAppAddedOrUpdated() {
    auto app_metadata = phonehub::Notification::AppMetadata(
        kAppName, kPackageName,
        /*color_icon=*/gfx::Image(), /*monochrome_icon_mask=*/std::nullopt,
        /*icon_color=*/std::nullopt,
        /*icon_is_monochrome=*/true, kUserId,
        phonehub::proto::AppStreamabilityStatus::STREAMABLE);

    fake_recent_apps_interaction_handler_.NotifyRecentAppAddedOrUpdated(
        app_metadata, base::Time::Now());
    fake_phone_hub_manager_.fake_app_stream_launcher_data_model()->AddAppToList(
        app_metadata);
  }

  void SimulateBubbleCloseAndOpen() {
    phone_hub_recent_apps_view_.reset();
    phone_hub_recent_apps_view_.release();
    phone_hub_recent_apps_view_ = std::make_unique<PhoneHubRecentAppsView>(
        &fake_recent_apps_interaction_handler_, &fake_phone_hub_manager_,
        connected_view_);
  }

  size_t PackageNameToClickCount(const std::string& package_name) {
    return fake_recent_apps_interaction_handler_.HandledRecentAppsCount(
        package_name);
  }

  void FeatureStateChanged(FeatureState feature_state) {
    fake_recent_apps_interaction_handler_.OnFeatureStateChanged(feature_state);
  }

  bool AppStreamLauncherShowState() {
    return fake_phone_hub_manager_.fake_app_stream_launcher_data_model()
        ->GetShouldShowMiniLauncher();
  }

  void SetRecentAppsHandlerUiState(RecentAppsUiState ui_state) {
    fake_recent_apps_interaction_handler_.set_ui_state_for_testing(ui_state);
  }

  views::View* GetLoadingView() {
    return phone_hub_recent_apps_view_->get_loading_view_for_test();
  }

  views::ImageButton* GetErrorButton() {
    return phone_hub_recent_apps_view_->get_error_button_for_test();
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<PhoneHubRecentAppsView> phone_hub_recent_apps_view_;
  phonehub::FakeRecentAppsInteractionHandler
      fake_recent_apps_interaction_handler_;
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  raw_ptr<PhoneConnectedView> connected_view_;
};

TEST_F(RecentAppButtonsViewTest, TaskViewVisibility) {
  // The recent app view is not visible if the NotifyRecentAppAddedOrUpdated
  // function never be called, e.g. device boot.
  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  // The feature state is enabled but no recent app has been added yet, we
  // should not show the recent app buttons view.
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(recent_apps_view()->recent_app_buttons_view_->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  // The feature state is disabled so we should not show all recent apps view.
  FeatureStateChanged(FeatureState::kDisabledByUser);
  recent_apps_view()->Update();

  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());
}

TEST_F(RecentAppButtonsViewTest,
       TaskViewVisibility_NetworkConnectionFlagDisabled) {
  feature_list_.Reset();
  feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kEcheLauncher, features::kEcheSWA,
                            features::kEcheLauncherIconsInMoreAppsButton},
      /*disabled_features=*/{features::kEcheNetworkConnectionState});

  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(recent_apps_view()->recent_app_buttons_view_->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  FeatureStateChanged(FeatureState::kDisabledByUser);
  recent_apps_view()->Update();

  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());
}

TEST_F(RecentAppButtonsViewTest, LoadingStateVisibility) {
  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  SetRecentAppsHandlerUiState(RecentAppsUiState::LOADING);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_TRUE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  SetRecentAppsHandlerUiState(RecentAppsUiState::ITEMS_VISIBLE);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());
  EXPECT_TRUE(recent_apps_view()->recent_app_buttons_view_->GetVisible());
}

TEST_F(RecentAppButtonsViewTest, ConnectionFailedStateVisibility) {
  EXPECT_FALSE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());

  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  SetRecentAppsHandlerUiState(RecentAppsUiState::CONNECTION_FAILED);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_TRUE(GetLoadingView()->GetVisible());
  EXPECT_TRUE(GetErrorButton()->GetVisible());

  SetRecentAppsHandlerUiState(RecentAppsUiState::ITEMS_VISIBLE);
  recent_apps_view()->Update();

  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_FALSE(GetLoadingView()->GetVisible());
  EXPECT_FALSE(GetErrorButton()->GetVisible());
  EXPECT_TRUE(recent_apps_view()->recent_app_buttons_view_->GetVisible());
}

TEST_F(RecentAppButtonsViewTest, SingleRecentAppButtonsView) {
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 1;
  EXPECT_TRUE(recent_apps_view()->GetVisible());
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());
}

TEST_F(RecentAppButtonsViewTest, MultipleRecentAppButtonsView) {
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 3;
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());

  for (views::View* child :
       recent_apps_view()->recent_app_buttons_view_->children()) {
    PhoneHubRecentAppButton* recent_app =
        static_cast<PhoneHubRecentAppButton*>(child);
    // Simulate clicking button using placeholder event.
    views::test::ButtonTestApi(recent_app).NotifyClick(ui::test::TestEvent());
  }

  size_t expected_number_of_button_be_clicked = 3;
  EXPECT_EQ(expected_number_of_button_be_clicked,
            PackageNameToClickCount(kPackageName));
}

TEST_F(RecentAppButtonsViewTest,
       MultipleRecentAppButtonsWithMoreAppsButtonView) {
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);
  recent_apps_view()->Update();

  size_t expected_recent_app_button = 7;
  EXPECT_EQ(expected_recent_app_button,
            recent_apps_view()->recent_app_buttons_view_->children().size());

  for (std::size_t i = 0;
       i != recent_apps_view()->recent_app_buttons_view_->children().size();
       i++) {
    auto* child =
        recent_apps_view()->recent_app_buttons_view_->children()[i].get();
    if (i == 6) {
      break;
    }
    PhoneHubRecentAppButton* recent_app =
        static_cast<PhoneHubRecentAppButton*>(child);
    // Simulate clicking button using placeholder event.
    views::test::ButtonTestApi(recent_app).NotifyClick(ui::test::TestEvent());
  }

  size_t expected_number_of_button_be_clicked = 6;
  EXPECT_EQ(expected_number_of_button_be_clicked,
            PackageNameToClickCount(kPackageName));
}

TEST_F(RecentAppButtonsViewTest, LogRecentAppsStateOnBubbleOpened) {
  base::HistogramTester histogram_tester;

  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);

  SetRecentAppsHandlerUiState(RecentAppsUiState::HIDDEN);
  SimulateBubbleCloseAndOpen();
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kLoading, 0);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kError, 0);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName, RecentAppsViewUiState::kApps,
      0);

  SetRecentAppsHandlerUiState(RecentAppsUiState::PLACEHOLDER_VIEW);
  SimulateBubbleCloseAndOpen();
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kLoading, 0);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kError, 0);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName, RecentAppsViewUiState::kApps,
      0);

  SetRecentAppsHandlerUiState(RecentAppsUiState::LOADING);
  SimulateBubbleCloseAndOpen();
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kLoading, 1);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kError, 0);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName, RecentAppsViewUiState::kApps,
      0);

  SetRecentAppsHandlerUiState(RecentAppsUiState::CONNECTION_FAILED);
  SimulateBubbleCloseAndOpen();
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kLoading, 1);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kError, 1);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName, RecentAppsViewUiState::kApps,
      0);

  SetRecentAppsHandlerUiState(RecentAppsUiState::ITEMS_VISIBLE);
  SimulateBubbleCloseAndOpen();
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kLoading, 1);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName,
      RecentAppsViewUiState::kError, 1);
  histogram_tester.ExpectBucketCount(
      kRecentAppsStateOnBubbleOpenedHistogramName, RecentAppsViewUiState::kApps,
      1);
}

TEST_F(RecentAppButtonsViewTest, LogRecentAppsTransitionToFailedLatency) {
  base::HistogramTester histogram_tester;

  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);

  SetRecentAppsHandlerUiState(RecentAppsUiState::LOADING);
  SimulateBubbleCloseAndOpen();
  SetRecentAppsHandlerUiState(RecentAppsUiState::CONNECTION_FAILED);
  recent_apps_view()->Update();

  histogram_tester.ExpectTimeBucketCount(
      kRecentAppsTransitionToFailedLatencyHistogramName, base::Milliseconds(0),
      1);
}

// TODO(crbug.com/1476926): Disabled due to flakiness.
TEST_F(RecentAppButtonsViewTest,
       DISABLED_LogRecentAppsTransitionToSuccessLatency) {
  base::HistogramTester histogram_tester;

  NotifyRecentAppAddedOrUpdated();
  FeatureStateChanged(FeatureState::kEnabledByUser);

  SetRecentAppsHandlerUiState(RecentAppsUiState::LOADING);
  SimulateBubbleCloseAndOpen();
  SetRecentAppsHandlerUiState(RecentAppsUiState::ITEMS_VISIBLE);
  recent_apps_view()->Update();

  histogram_tester.ExpectTimeBucketCount(
      kRecentAppsTransitionToSuccessLatencyHistogramName, base::Milliseconds(0),
      1);

  SetRecentAppsHandlerUiState(RecentAppsUiState::CONNECTION_FAILED);
  SimulateBubbleCloseAndOpen();
  SetRecentAppsHandlerUiState(RecentAppsUiState::ITEMS_VISIBLE);
  recent_apps_view()->Update();

  histogram_tester.ExpectTimeBucketCount(
      kRecentAppsTransitionToSuccessLatencyHistogramName, base::Milliseconds(0),
      2);
}

}  // namespace ash
