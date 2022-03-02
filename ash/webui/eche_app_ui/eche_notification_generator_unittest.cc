// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_notification_generator.h"

#include "ash/components/phonehub/fake_phone_hub_manager.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

class MockLaunchAppHelper : public LaunchAppHelper {
 public:
  MockLaunchAppHelper(phonehub::PhoneHubManager* phone_hub_manager,
                      LaunchEcheAppFunction launch_eche_app_function,
                      CloseEcheAppFunction close_eche_app_function,
                      LaunchNotificationFunction launch_notification_function)
      : LaunchAppHelper(phone_hub_manager,
                        launch_eche_app_function,
                        close_eche_app_function,
                        launch_notification_function) {}

  ~MockLaunchAppHelper() override = default;
  MockLaunchAppHelper(const MockLaunchAppHelper&) = delete;
  MockLaunchAppHelper& operator=(const MockLaunchAppHelper&) = delete;

  // LaunchAppHelper:
  MOCK_METHOD(void,
              ShowNotification,
              (const absl::optional<std::u16string>& title,
               const absl::optional<std::u16string>& message,
               std::unique_ptr<NotificationInfo> info),
              (const, override));
};

class EcheNotificationGeneratorTest : public testing::Test {
 protected:
  EcheNotificationGeneratorTest() = default;
  EcheNotificationGeneratorTest(const EcheNotificationGeneratorTest&) = delete;
  EcheNotificationGeneratorTest& operator=(
      const EcheNotificationGeneratorTest&) = delete;
  ~EcheNotificationGeneratorTest() override = default;

  // testing::Test:
  void SetUp() override {
    launch_app_helper_ = std::make_unique<MockLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(
            &EcheNotificationGeneratorTest::FakeLaunchEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationGeneratorTest::FakeCloseEcheAppFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheNotificationGeneratorTest::FakeLaunchNotificationFunction,
            base::Unretained(this)));
    notification_generator_ =
        std::make_unique<EcheNotificationGenerator>(launch_app_helper_.get());
  }

  void TearDown() override {
    launch_app_helper_.reset();
    notification_generator_.reset();
  }

  void FakeLaunchEcheAppFunction(const absl::optional<int64_t>& notification_id,
                                 const std::string& package_name,
                                 const std::u16string& visible_name,
                                 const absl::optional<int64_t>& user_id,
                                 const gfx::Image& icon) {
    // Do nothing.
  }

  void FakeLaunchNotificationFunction(
      const absl::optional<std::u16string>& title,
      const absl::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    // Do nothing.
  }

  void FakeCloseEcheAppFunction() {
    // Do nothing.
  }

  void ShowNotification(const std::u16string& title,
                        const std::u16string& message,
                        mojom::WebNotificationType type) {
    notification_generator_->ShowNotification(title, message, type);
  }

  std::unique_ptr<MockLaunchAppHelper> launch_app_helper_;

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  std::unique_ptr<EcheNotificationGenerator> notification_generator_;
};

TEST_F(EcheNotificationGeneratorTest, ShowNotification) {
  const absl::optional<std::u16string> title = u"title";
  const absl::optional<std::u16string> message = u"message";

  // APP_CRAHSED
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::APP_CRAHSED);
  // AUTHORIZATION_NEEDED
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::AUTHORIZATION_NEEDED);
  // CONNECTION_FAILED
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::CONNECTION_FAILED);
  // CONNECTION_LOST
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::CONNECTION_LOST);
  // DEVICE_IDLE
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::DEVICE_IDLE);
  // INITIALIZATION_ERROR
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::INITIALIZATION_ERROR);
  // INVALID_NOTIFICATION
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::INVALID_NOTIFICATION);
  // LAUNCH_NOTIFICATION_FAILED
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::LAUNCH_NOTIFICATION_FAILED);
  // TABLET_MODE
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::TABLET_MODE);
}

}  // namespace eche_app
}  // namespace ash
