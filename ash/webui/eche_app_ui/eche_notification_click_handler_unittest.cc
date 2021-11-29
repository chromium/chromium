// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_notification_click_handler.h"

#include <string>

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/constants/ash_features.h"
#include "ash/webui/eche_app_ui/fake_feature_status_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace eche_app {

class TestableLaunchAppHelper : public LaunchAppHelper {
 public:
  TestableLaunchAppHelper(
      phonehub::PhoneHubManager* phone_hub_manager,
      LaunchEcheAppFunction launch_eche_app_function,
      CloseEcheAppFunction close_eche_app_function,
      LaunchNotificationFunction launch_notification_function)
      : LaunchAppHelper(phone_hub_manager,
                        launch_eche_app_function,
                        close_eche_app_function,
                        launch_notification_function) {}

  ~TestableLaunchAppHelper() override = default;
  TestableLaunchAppHelper(const TestableLaunchAppHelper&) = delete;
  TestableLaunchAppHelper& operator=(const TestableLaunchAppHelper&) = delete;
  // LaunchAppHelper:
  LaunchAppHelper::AppLaunchProhibitedReason checkAppLaunchProhibitedReason(
      FeatureStatus status) const override {
    return LaunchAppHelper::AppLaunchProhibitedReason::kNotProhibited;
  }
  void ShowNotification(const absl::optional<std::u16string>& title,
                        const absl::optional<std::u16string>& message,
                        std::unique_ptr<NotificationInfo> info) const override {
    // Do nothing.
  }
};

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
    scoped_feature_list_.InitWithFeatures({features::kEcheSWA}, {});
    launch_app_helper_ = std::make_unique<TestableLaunchAppHelper>(
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
    handler_ = std::make_unique<EcheNotificationClickHandler>(
        &fake_phone_hub_manager_, &fake_feature_status_provider_,
        launch_app_helper_.get());
  }

  void TearDown() override {
    launch_app_helper_.reset();
    handler_.reset();
  }

  void FakeLaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                                 const std::string& package_name,
                                 const std::u16string& visible_name,
                                 const absl::optional<int64_t>& user_id) {
    // Do nothing.
  }

  void FakeLaunchNotificationFunction(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    // Do nothing.
  }

  void FakeCloseEcheAppFunction() { close_eche_is_called_ = true; }

  void SetStatus(FeatureStatus status) {
    fake_feature_status_provider_.SetStatus(status);
  }

  size_t GetNumberOfClickHandlers() {
    return fake_phone_hub_manager_.fake_notification_interaction_handler()
        ->notification_click_handler_count();
  }

  bool getCloseEcheAppFlag() { return close_eche_is_called_; }

  void resetCloseEcheAppFlag() { close_eche_is_called_ = false; }

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;
  FakeFeatureStatusProvider fake_feature_status_provider_;
  std::unique_ptr<LaunchAppHelper> launch_app_helper_;
  std::unique_ptr<EcheNotificationClickHandler> handler_;
  bool close_eche_is_called_;
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
  EXPECT_EQ(true, getCloseEcheAppFlag());

  resetCloseEcheAppFlag();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDisabled);
  EXPECT_EQ(true, getCloseEcheAppFlag());

  resetCloseEcheAppFlag();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDependentFeature);
  EXPECT_EQ(true, getCloseEcheAppFlag());

  resetCloseEcheAppFlag();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kNotEnabledByPhone);
  EXPECT_EQ(true, getCloseEcheAppFlag());

  resetCloseEcheAppFlag();
  SetStatus(FeatureStatus::kDisconnected);
  SetStatus(FeatureStatus::kDependentFeaturePending);
  EXPECT_EQ(false, getCloseEcheAppFlag());
}
}  // namespace eche_app
}  // namespace ash
