// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_ui_controller.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/fake_tether_controller.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/view.h"

namespace ash {

using FeatureStatus = phonehub::FeatureStatus;
using TetherStatus = phonehub::TetherController::Status;

constexpr char kUser1Email[] = "user1@test.com";
constexpr char kUser2Email[] = "user2@test.com";
constexpr char kScreenOnOpenedMetric[] =
    "PhoneHub.BubbleOpened.Connectable.Page";
constexpr base::TimeDelta kConnectingViewGracePeriod = base::Seconds(40);

class PhoneHubUiControllerTest : public AshTestBase,
                                 public PhoneHubUiController::Observer {
 public:
  PhoneHubUiControllerTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    set_start_session(false);
  }

  ~PhoneHubUiControllerTest() override { controller_->RemoveObserver(this); }

  // AshTestBase:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kEcheSWA, features::kEcheNetworkConnectionState}, {});

    AshTestBase::SetUp();

    handler_ = std::make_unique<eche_app::EcheConnectionStatusHandler>();
    phone_hub_manager_.set_host_last_seen_timestamp(std::nullopt);
    phone_hub_manager_.set_eche_connection_handler(handler_.get());

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);
    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    controller_ = std::make_unique<PhoneHubUiController>();
    controller_->AddObserver(this);

    GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
    GetOnboardingUiTracker()->SetShouldShowOnboardingUi(false);
    controller_->SetPhoneHubManager(&phone_hub_manager_);

    CHECK(ui_state_changed_);
    ui_state_changed_ = false;
  }

  void SetLoggedInUser(bool is_primary) {
    const std::string& email = is_primary ? kUser1Email : kUser2Email;
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  phonehub::FakeFeatureStatusProvider* GetFeatureStatusProvider() {
    return phone_hub_manager_.fake_feature_status_provider();
  }

  phonehub::FakeOnboardingUiTracker* GetOnboardingUiTracker() {
    return phone_hub_manager_.fake_onboarding_ui_tracker();
  }

  phonehub::FakeTetherController* GetTetherController() {
    return phone_hub_manager_.fake_tether_controller();
  }

  void SetPhoneStatusModel(
      const std::optional<phonehub::PhoneStatusModel>& phone_status_model) {
    phone_hub_manager_.mutable_phone_model()->SetPhoneStatusModel(
        phone_status_model);
  }

  std::unique_ptr<PhoneHubContentView> OpenBubbleAndCreateView() {
    controller_->HandleBubbleOpened();
    return controller_->CreateContentView(/*delegate=*/nullptr);
  }

  void CallHandleBubbleOpened() { controller_->HandleBubbleOpened(); }

  // When first connecting, the connecting view is shown for 30 seconds when
  // disconnected, so in order to show the disconnecting view, we need to fast
  // forward time.
  void FastForwardByConnectingViewGracePeriod() {
    task_environment()->FastForwardBy(kConnectingViewGracePeriod);
  }

 protected:
  // PhoneHubUiController::Observer:
  void OnPhoneHubUiStateChanged() override {
    CHECK(!ui_state_changed_);
    ui_state_changed_ = true;
  }

  std::unique_ptr<eche_app::EcheConnectionStatusHandler> handler_;
  phonehub::FakePhoneHubManager phone_hub_manager_;
  std::unique_ptr<PhoneHubUiController> controller_;
  bool ui_state_changed_ = false;

 private:
  base::test::ScopedFeatureList feature_list_;

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(PhoneHubUiControllerTest, NotEligibleForFeature) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kNotEligibleForFeature);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_TRUE(ui_state_changed_);
  EXPECT_FALSE(OpenBubbleAndCreateView().get());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, OnboardingNotEligible) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(OpenBubbleAndCreateView().get());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithoutPhone) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kEligiblePhoneButNotSetUp);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithoutPhone,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, ShowOnboardingUi_WithPhone) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kDisabled);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetOnboardingUiTracker()->SetShouldShowOnboardingUi(true);
  EXPECT_TRUE(ui_state_changed_);

  EXPECT_EQ(PhoneHubUiController::UiState::kOnboardingWithPhone,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kOnboardingView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, PhoneSelectedAndPendingSetup) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kPhoneSelectedAndPendingSetup);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, BluetoothOff) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kUnavailableBluetoothOff);
  EXPECT_EQ(PhoneHubUiController::UiState::kBluetoothDisabled,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kBluetoothDisabledView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, PhoneConnecting_DiscoveredRecently) {
  base::HistogramTester histograms;
  phone_hub_manager_.set_host_last_seen_timestamp(base::Time::Now());
  GetTetherController()->SetStatus(
      phonehub::TetherController::Status::kConnectionAvailable);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view->GetID());
  histograms.ExpectBucketCount(kScreenOnOpenedMetric,
                               phone_hub_metrics::Screen::kPhoneConnecting, 1);
}

TEST_F(PhoneHubUiControllerTest, PhoneConnecting_DiscoveredHoursAgo) {
  base::HistogramTester histograms;
  phone_hub_manager_.set_host_last_seen_timestamp(base::Time::Now() -
                                                  base::Hours(10));
  GetTetherController()->SetStatus(
      phonehub::TetherController::Status::kConnectionAvailable);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, PhoneConnecting_NeverDiscovered) {
  base::HistogramTester histograms;
  GetTetherController()->SetStatus(
      phonehub::TetherController::Status::kConnectionAvailable);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, TetherConnectionPending) {
  base::HistogramTester histograms;
  GetTetherController()->SetStatus(
      phonehub::TetherController::Status::kConnecting);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  // Tether status becomes connected, but the feature status is still
  // |kEnabledAndConnecting|. The UiState should still be
  // kTetherConnectionPending.
  GetTetherController()->SetStatus(
      phonehub::TetherController::Status::kConnected);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  // Tether status is connected, the feature status is |kEnabledAndConnected|,
  // but there is no phone model. The UiState should still be
  // kTetherConnectionPending.
  SetPhoneStatusModel(std::nullopt);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kTetherConnectionPending,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kTetherConnectionPendingView,
            content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, PhoneConnected) {
  base::HistogramTester histograms;
  SetPhoneStatusModel(phonehub::CreateFakePhoneStatusModel());
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnected,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(kPhoneConnectedView, content_view->GetID());
  histograms.ExpectBucketCount(kScreenOnOpenedMetric,
                               phone_hub_metrics::Screen::kPhoneConnected, 1);
}

TEST_F(PhoneHubUiControllerTest, UnavailableScreenLocked) {
  base::HistogramTester histograms;
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kLockOrSuspended);
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(OpenBubbleAndCreateView().get());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, UnavailableSecondaryUser) {
  base::HistogramTester histograms;
  // Simulate log in to secondary user.
  SetLoggedInUser(false /* is_primary */);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  EXPECT_EQ(PhoneHubUiController::UiState::kHidden, controller_->ui_state());
  EXPECT_FALSE(OpenBubbleAndCreateView().get());

  // Switch back to primary user.
  SetLoggedInUser(true /* is_primary */);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);
}

TEST_F(PhoneHubUiControllerTest, ConnectedViewDelayed) {
  base::HistogramTester histograms;
  // Since there is no phone model, expect that we stay at the connecting screen
  // even though the feature status is kEnabledAndConnected.
  SetPhoneStatusModel(std::nullopt);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnecting,
            controller_->ui_state());
  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(kPhoneConnectingView, content_view->GetID());
  histograms.ExpectTotalCount(kScreenOnOpenedMetric, 0);

  // Update the phone status model and expect the connected view to show up.
  SetPhoneStatusModel(phonehub::CreateFakePhoneStatusModel());
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneConnected,
            controller_->ui_state());
  auto content_view2 = OpenBubbleAndCreateView();
  EXPECT_EQ(kPhoneConnectedView, content_view2->GetID());
  histograms.ExpectBucketCount(kScreenOnOpenedMetric,
                               phone_hub_metrics::Screen::kPhoneConnected, 1);
}

TEST_F(PhoneHubUiControllerTest, NumScanForAvailableConnectionCalls) {
  size_t num_scan_for_connection_calls =
      GetTetherController()->num_scan_for_available_connection_calls();

  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnected);
  GetTetherController()->SetStatus(TetherStatus::kConnectionUnavailable);

  // A scan for available connection calls should occur the first time
  // the PhoneHub UI is opened while the feature status is enabled
  // and the tether status is kConnectionUnavailable.
  CallHandleBubbleOpened();
  EXPECT_EQ(GetTetherController()->num_scan_for_available_connection_calls(),
            num_scan_for_connection_calls + 1);

  // No scan for available connection calls should occur after a tether scan
  // has been requested.
  CallHandleBubbleOpened();
  EXPECT_EQ(GetTetherController()->num_scan_for_available_connection_calls(),
            num_scan_for_connection_calls + 1);
}

TEST_F(PhoneHubUiControllerTest,
       DisconnectedViewWhenDisconnectedGreaterThan30Seconds) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  FastForwardByConnectingViewGracePeriod();
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneDisconnected,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kDisconnectedView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest,
       ConnectingViewWhenDisconnectedLessThan30Seconds) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledAndConnecting);
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_EQ(PhoneHubUiController::UiState::kPhoneDisconnected,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kPhoneConnectingView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, TimerExpiresBluetoothDisconnectedView) {
  GetFeatureStatusProvider()->SetStatus(FeatureStatus::kEnabledButDisconnected);
  EXPECT_TRUE(ui_state_changed_);
  ui_state_changed_ = false;
  GetFeatureStatusProvider()->SetStatus(
      FeatureStatus::kUnavailableBluetoothOff);
  FastForwardByConnectingViewGracePeriod();
  EXPECT_EQ(PhoneHubUiController::UiState::kBluetoothDisabled,
            controller_->ui_state());

  auto content_view = OpenBubbleAndCreateView();
  EXPECT_EQ(PhoneHubViewID::kBluetoothDisabledView, content_view->GetID());
}

TEST_F(PhoneHubUiControllerTest, HandleBubbleOpenedShouldCloseEcheBubble) {
  EcheTray* eche_tray =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();
  eche_tray->LoadBubble(
      GURL("http://google.com"), gfx::Image(), u"app 1", u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::APPS_LIST);
  eche_tray->ShowBubble();
  EXPECT_TRUE(
      eche_tray->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());

  controller_->HandleBubbleOpened();

  EXPECT_FALSE(
      eche_tray->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
}

}  // namespace ash
