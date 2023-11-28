// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/eche_alert_generator.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "ash/webui/eche_app_ui/apps_launch_info_provider.h"
#include "ash/webui/eche_app_ui/launch_app_helper.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace eche_app {

class MockLaunchAppHelper : public LaunchAppHelper {
 public:
  MockLaunchAppHelper(phonehub::PhoneHubManager* phone_hub_manager,
                      LaunchEcheAppFunction launch_eche_app_function,
                      LaunchNotificationFunction launch_notification_function,
                      CloseNotificationFunction close_notification_function)
      : LaunchAppHelper(phone_hub_manager,
                        launch_eche_app_function,
                        launch_notification_function,
                        close_notification_function) {}
  ~MockLaunchAppHelper() override = default;
  MockLaunchAppHelper(const MockLaunchAppHelper&) = delete;
  MockLaunchAppHelper& operator=(const MockLaunchAppHelper&) = delete;

  // LaunchAppHelper:
  MOCK_METHOD(void,
              ShowNotification,
              (const std::optional<std::u16string>& title,
               const std::optional<std::u16string>& message,
               std::unique_ptr<NotificationInfo> info),
              (const, override));

  MOCK_METHOD(void, ShowToast, (const std::u16string& text), (const, override));

  MOCK_METHOD(void,
              CloseNotification,
              (const std::string& notification_id),
              (const, override));
};

class EcheAlertGeneratorTest : public testing::Test {
 protected:
  EcheAlertGeneratorTest() = default;
  EcheAlertGeneratorTest(const EcheAlertGeneratorTest&) = delete;
  EcheAlertGeneratorTest& operator=(const EcheAlertGeneratorTest&) = delete;
  ~EcheAlertGeneratorTest() override = default;

  // testing::Test:
  void SetUp() override {
    launch_app_helper_ = std::make_unique<MockLaunchAppHelper>(
        &fake_phone_hub_manager_,
        base::BindRepeating(&EcheAlertGeneratorTest::FakeLaunchEcheAppFunction,
                            base::Unretained(this)),
        base::BindRepeating(
            &EcheAlertGeneratorTest::FakeLaunchNotificationFunction,
            base::Unretained(this)),
        base::BindRepeating(
            &EcheAlertGeneratorTest::FakeCloseNotificationFunction,
            base::Unretained(this)));
    alert_generator_ = std::make_unique<EcheAlertGenerator>(
        launch_app_helper_.get(), &pref_service_);
    pref_service_.registry()->RegisterBooleanPref(
        ash::prefs::kEnableAutoScreenLock, false);
  }

  void TearDown() override {
    launch_app_helper_.reset();
    alert_generator_.reset();
  }

  void FakeLaunchEcheAppFunction(
      const std::optional<int64_t>& notification_id,
      const std::string& package_name,
      const std::u16string& visible_name,
      const std::optional<int64_t>& user_id,
      const gfx::Image& icon,
      const std::u16string& phone_name,
      AppsLaunchInfoProvider* apps_launch_info_provider) {
    // Do nothing.
  }

  void FakeLaunchNotificationFunction(
      const std::optional<std::u16string>& title,
      const std::optional<std::u16string>& message,
      std::unique_ptr<LaunchAppHelper::NotificationInfo> info) {
    // Do nothing.
  }

  void FakeCloseNotificationFunction(const std::string& notification_id) {
    // Do nothing.
  }

  void ShowNotification(const std::u16string& title,
                        const std::u16string& message,
                        mojom::WebNotificationType type) {
    alert_generator_->ShowNotification(title, message, type);
  }

  void ShowToast(const std::u16string& text) {
    alert_generator_->ShowToast(text);
  }

  void TriggerOnEnableScreenLockChanged() {
    // Trigger observer callback by changing kEnableAutoScreenLock value.
    pref_service_.SetBoolean(ash::prefs::kEnableAutoScreenLock, true);
  }

  std::unique_ptr<MockLaunchAppHelper> launch_app_helper_;

 private:
  phonehub::FakePhoneHubManager fake_phone_hub_manager_;
  std::unique_ptr<EcheAlertGenerator> alert_generator_;
  TestingPrefServiceSimple pref_service_;
};

TEST_F(EcheAlertGeneratorTest, ShowNotification) {
  const std::optional<std::u16string> title = u"title";
  const std::optional<std::u16string> message = u"message";

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
  // WIFI_NOT_READY
  EXPECT_CALL(*launch_app_helper_,
              ShowNotification(testing::_, testing::_, testing::_));
  ShowNotification(title.value(), message.value(),
                   mojom::WebNotificationType::WIFI_NOT_READY);
}

TEST_F(EcheAlertGeneratorTest, ShowToast) {
  std::u16string text = u"text";

  EXPECT_CALL(*launch_app_helper_, ShowToast(testing::_));
  ShowToast(text);
}

TEST_F(EcheAlertGeneratorTest, VerifyCloseScreenLockNotification) {
  EXPECT_CALL(*launch_app_helper_, CloseNotification(testing::_));
  TriggerOnEnableScreenLockChanged();
}

}  // namespace eche_app
}  // namespace ash
