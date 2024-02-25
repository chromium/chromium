// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"

#include <memory>
#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/eche_connection_status_handler.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/fake_launch_app_helper.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

class EcheNotificationClickHandlerTest : public testing::Test {
 protected:
  EcheNotificationClickHandlerTest() = default;
  EcheNotificationClickHandlerTest(const EcheNotificationClickHandlerTest&) =
      delete;
  EcheNotificationClickHandlerTest& operator=(
      const EcheNotificationClickHandlerTest&) = delete;
  ~EcheNotificationClickHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    fake_phone_hub_manager_.fake_feature_status_provider()->SetStatus(
        phonehub::FeatureStatus::kEnabledAndConnected);
    fake_feature_status_provider_.SetStatus(FeatureStatus::kIneligible);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
    launch_app_helper_ = std::make_unique<FakeLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeLaunchNotificationFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationClickHandlerTest::FakeCloseNotificationFunction,
            base::Unretained(this)));
    connection_status_handler_ =
        std::make_unique<eche_app::EcheConnectionStatusHandler>();
    apps_launch_info_provider_ = std::make_unique<AppsLaunchInfoProvider>(
        connection_status_handler_.get());
    handler_ = std::make_unique<EcheNotificationClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        launch_app_helper_.get(), apps_launch_info_provider_.get());
  }

  void TearDown() override {
    apps_launch_info_provider_.reset();
    connection_status_handler_.reset();
    launch_app_helper_.reset();
    handler_.reset();
  }

  void FakeLaunchEcheAppFunction(
      const std::optional<int64_t>& notification_id,
      const std::string& package_name,
      const std::u16string& visible_name,
      const std::optional<int64_t>& user_id,
      const gfx::Image& icon,
      const std::u16string& phone_name,
      AppsLaunchInfoProvider* apps_launch_info_provider) {
    num_app_launch_++;
  }

  void FakeLaunchNotificationFunction(
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    num_notifications_shown_++;
  }

  void FakeCloseNotificationFunction(const std::string& notification_id) {
    // Do nothing.
  }

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

  size_t num_notifications_shown() { return num_notifications_shown_; }

  size_t num_app_launch() { return num_app_launch_; }

  void reset() {
    num_notifications_shown_ = 0;
    num_app_launch_ = 0;
  }

  std::unique_ptr<EcheNotificationClickHandler> handler_;

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<FakeLaunchAppHelper> launch_app_helper_;
  std::unique_ptr<eche_app::EcheConnectionStatusHandler>
      connection_status_handler_;
  std::unique_ptr<AppsLaunchInfoProvider> apps_launch_info_provider_;
  size_t num_notifications_shown_ = 0;
  size_t num_app_launch_ = 0;
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
}

TEST_F(EcheNotificationClickHandlerTest, HandleNotificationClick) {
  const int64_t notification_id = 0;
  const char16_t app_name[] = u"Test App";
  const char package_name[] = "com.google.testapp";
  const int64_t user_id = 0;
  base::HistogramTester histogram_tester;
  phonehub::Notification::AppMetadata app_meta_data =
      phonehub::Notification::AppMetadata(app_name, package_name,
                                          /*color_icon=*/gfx::Image(),
                                          /*monochrome_icon_mask=*/gfx::Image(),
                                          /*icon_color=*/std::nullopt,
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
  histogram_tester.ExpectUniqueSample(
      "Eche.AppStream.LaunchAttempt",
      mojom::AppStreamLaunchEntryPoint::NOTIFICATION, 1);
}

}  // namespace eche_app
}  // namespace ash
