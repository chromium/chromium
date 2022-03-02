// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"

#include <string>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_suite.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/fake_launch_app_helper.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace eche_app {

class EcheNotificationClickHandlerTest : public AshTestBase {
 protected:
  EcheNotificationClickHandlerTest() = default;
  EcheNotificationClickHandlerTest(const EcheNotificationClickHandlerTest&) =
      delete;
  EcheNotificationClickHandlerTest& operator=(
      const EcheNotificationClickHandlerTest&) = delete;
  ~EcheNotificationClickHandlerTest() override = default;

  // AshTestBase::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA, features::kEcheCustomWidget},
        /*disabled_features=*/{});

    DCHECK(test_web_view_factory_.get());

    ui::ResourceBundle::CleanupSharedInstance();
    AshTestSuite::LoadTestResources();
    AshTestBase::SetUp();
    eche_tray_ =
        ash::StatusAreaWidgetTestHelper::GetStatusAreaWidget()->eche_tray();

    fake_phone_hub_manager_.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    fake_feature_status_provider_.SetStatus(FeatureStatus::kIneligible);
    launch_app_helper_ = std::make_unique<FakeLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeCloseEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeLaunchNotificationFunction,
            base::Unretained(this)));
    display_stream_handler_ = std::make_unique<EcheDisplayStreamHandler>();
    handler_ = std::make_unique<EcheNotificationClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        launch_app_helper_.get(), display_stream_handler_.get());
  }

  void TearDown() override {
    AshTestBase::TearDown();
    launch_app_helper_.reset();
    display_stream_handler_.reset();
    handler_.reset();
  }

  void FakeLaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                                 const std::string& package_name,
                                 const std::u16string& visible_name,
                                 const absl::optional<int64_t>& user_id) {
    num_app_launch_++;
  }

  void FakeLaunchNotificationFunction(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    num_notifications_shown_++;
  }

  void FakeCloseEcheAppFunction() { close_eche_is_called_ = true; }

  void SetStatus(FeatureStatus status) {
    fake_feature_status_provider_.SetStatus(status);
  }

  void SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason reason) {
    launch_app_helper_->SetAppLaunchProhibitedReason(reason);
  }

  void HandleNotificationClick(
      int64_t notification_id,
      const phonehub::Notification::AppMetadata& app_metadata) {
    handler_->HandleNotificationClick(notification_id, app_metadata);
  }

  size_t GetNumberOfClickHandlers() {
    return fake_phone_hub_manager_.fake_notification_interaction_handler()
        ->notification_click_handler_count();
  }

  void StartStreaming() { handler_->OnStartStreaming(); }

  bool close_eche_is_called() { return close_eche_is_called_; }

  size_t num_notifications_shown() { return num_notifications_shown_; }

  size_t num_app_launch() { return num_app_launch_; }

  bool waiting_for_streaming_to_show() {
    return handler_->waiting_for_streaming_to_show();
  }

  EcheTray* eche_tray() { return eche_tray_; }

  void reset() {
    close_eche_is_called_ = false;
    num_notifications_shown_ = 0;
    num_app_launch_ = 0;
  }

  std::unique_ptr<EcheNotificationClickHandler> handler_;

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<FakeLaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheDisplayStreamHandler> display_stream_handler_;
  bool close_eche_is_called_;
  size_t num_notifications_shown_ = 0;
  size_t num_app_launch_ = 0;
  EcheTray* eche_tray_ = nullptr;  // Not owned

  // Calling the factory constructor is enough to set it up.
  std::unique_ptr<TestAshWebViewFactory> test_web_view_factory_ =
      std::make_unique<TestAshWebViewFactory>();
};

TEST_F(EcheNotificationClickHandlerTest, StatusChangeTransitions) {
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kConnecting);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kConnected);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDisconnected);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(0u, GetNumberOfClickHandlers());
  SetStatus(FeatureStatus::kNotEnabledByPhone);
  EXPECT_EQ(1u, GetNumberOfClickHandlers());
}

TEST_F(EcheNotificationClickHandlerTest,
       StatusChangeTransitionsAndCloseEcheWindow) {
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kIneligible);
  EXPECT_EQ(true, close_eche_is_called());

  reset();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(true, close_eche_is_called());

  reset();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(true, close_eche_is_called());

  reset();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kNotEnabledByPhone);
  EXPECT_EQ(true, close_eche_is_called());

  reset();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(false, close_eche_is_called());
}

TEST_F(EcheNotificationClickHandlerTest, HandleNotificationClick) {
  const int64_t notification_id = 0;
  const char16_t app_name[] = u"Test App";
  const char package_name[] = "com.google.testapp";
  const int64_t user_id = 0;
  phonehub::Notification::AppMetadata app_meta_data =
      phonehub::Notification::AppMetadata(app_name, package_name,
                                          /*icon=*/gfx::Image(),
                                          /*icon_color=*/absl::nullopt,
                                          /*icon_is_monochrome=*/true, user_id);
  HandleNotificationClick(notification_id, app_meta_data);
  EXPECT_EQ(num_app_launch(), 1u);
  EXPECT_EQ(num_notifications_shown(), 0u);

  reset();
  SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByScreenLock);
  HandleNotificationClick(notification_id, app_meta_data);
  EXPECT_EQ(num_app_launch(), 0u);
  EXPECT_EQ(num_notifications_shown(), 1u);

  reset();
  SetAppLaunchProhibitedReason(
      LaunchAppHelper::AppLaunchProhibitedReason::kDisabledByPhone);
  HandleNotificationClick(notification_id, app_meta_data);
  EXPECT_EQ(num_app_launch(), 0u);
  EXPECT_EQ(num_notifications_shown(), 1u);
}

TEST_F(EcheNotificationClickHandlerTest, StartStreaming) {
  EXPECT_FALSE(waiting_for_streaming_to_show());

  const int64_t notification_id = 0;
  const char16_t app_name[] = u"Test App";
  const char package_name[] = "com.google.testapp";
  const int64_t user_id = 0;
  phonehub::Notification::AppMetadata app_meta_data =
      phonehub::Notification::AppMetadata(app_name, package_name,
                                          /*icon=*/gfx::Image(),
                                          /*icon_color=*/absl::nullopt,
                                          /*icon_is_monochrome=*/true, user_id);
  HandleNotificationClick(notification_id, app_meta_data);

  EXPECT_TRUE(waiting_for_streaming_to_show());

  StartStreaming();

  EXPECT_TRUE(
      eche_tray()->get_bubble_wrapper_for_test()->bubble_view()->GetVisible());
  EXPECT_FALSE(waiting_for_streaming_to_show());
}

}  // namespace eche_app
}  // namespace ash
