// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/firmware_update/firmware_update_notification_controller.h"

#include <memory>
#include <optional>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/fwupd/fwupd_client.h"
#include "chromeos/ash/components/fwupd/firmware_update_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

using ::message_center::MessageCenter;

const char kFirmwareUpdateNotificationId[] =
    "cros_firmware_update_notification_id";

}  // namespace

class FirmwareUpdateNotificationControllerTest : public AshTestBase {
 public:
  FirmwareUpdateNotificationControllerTest() = default;
  FirmwareUpdateNotificationControllerTest(
      const FirmwareUpdateNotificationControllerTest&) = delete;
  FirmwareUpdateNotificationControllerTest& operator=(
      const FirmwareUpdateNotificationControllerTest&) = delete;
  ~FirmwareUpdateNotificationControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    // Call NotifyFirstSessionReady to cause FirmwareUpdateManager to be
    // initialized since it is only meant to be initialized after core startup
    // tasks have been completed.
    Shell::Get()->session_controller()->NotifyFirstSessionReady();
  }

  FirmwareUpdateNotificationController* controller() {
    return Shell::Get()->firmware_update_notification_controller();
  }

  message_center::Notification* GetFirmwareUpdateNotification() {
    return MessageCenter::Get()->FindVisibleNotificationById(
        kFirmwareUpdateNotificationId);
  }

  int GetNumFirmwareUpdateUIOpened() {
    return GetSystemTrayClient()->show_firmware_update_count();
  }

  void ClickNotification(std::optional<int> button_index) {
    // No button index means the notification body was clicked.
    if (!button_index.has_value()) {
      message_center::Notification* notification =
          MessageCenter::Get()->FindVisibleNotificationById(
              kFirmwareUpdateNotificationId);
      notification->delegate()->Click(std::nullopt, std::nullopt);
      return;
    }

    message_center::Notification* notification =
        MessageCenter::Get()->FindVisibleNotificationById(
            kFirmwareUpdateNotificationId);
    notification->delegate()->Click(button_index, std::nullopt);
  }
};

TEST_F(FirmwareUpdateNotificationControllerTest, FirmwareUpdateNotification) {
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  controller()->NotifyFirmwareUpdateAvailable();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());

  message_center::Notification* notification = GetFirmwareUpdateNotification();

  EXPECT_TRUE(notification);

  // Ensure this notification has one button.
  EXPECT_EQ(1u, notification->buttons().size());

  EXPECT_EQ(0, GetNumFirmwareUpdateUIOpened());
  // Click on the update button and expect it to open the Firmware Update
  // SWA.
  ClickNotification(/*button_index=*/0);
  EXPECT_EQ(1, GetNumFirmwareUpdateUIOpened());
  // Clicking on the notification will close it.
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());

  // Open new notification and click on its body.
  controller()->NotifyFirmwareUpdateAvailable();
  EXPECT_EQ(1u, MessageCenter::Get()->NotificationCount());
  ClickNotification(std::nullopt);
  EXPECT_EQ(2, GetNumFirmwareUpdateUIOpened());
  EXPECT_EQ(0u, MessageCenter::Get()->NotificationCount());
}

class FirmwareUpdateStartupNotificationTest : public NoSessionAshTestBase {
 public:
  FirmwareUpdateStartupNotificationTest() = default;

  ~FirmwareUpdateStartupNotificationTest() override = default;

  void SetUp() override {
    network_handler_test_helper_.RegisterPrefs(profile_prefs_.registry(),
                                               local_state_.registry());

    network_handler_test_helper_.InitializePrefs(&profile_prefs_,
                                                 &local_state_);
    FwupdClient::InitializeFake();
    dbus_client_ = FwupdClient::Get();
    firmware_update_manager_ = std::make_unique<FirmwareUpdateManager>();
    EXPECT_TRUE(FirmwareUpdateManager::IsInitialized());
    SetShouldShowNotificationForTest(true);
    NoSessionAshTestBase::SetUp();
  }

  void TearDown() override {
    firmware_update_notification_controller_.reset();
    firmware_update_manager_.reset();
    FwupdClient::Shutdown();
    NetworkHandler::Get()->ShutdownPrefServices();
    NoSessionAshTestBase::TearDown();
  }

 protected:
  void InitializeNotificationController() {
    firmware_update_notification_controller_ =
        std::make_unique<FirmwareUpdateNotificationController>(
            message_center());
  }

  message_center::MessageCenter* message_center() const {
    return message_center::MessageCenter::Get();
  }

  message_center::Notification* FindShortcutsChangedNotification() {
    return message_center::MessageCenter::Get()->FindVisibleNotificationById(
        kFirmwareUpdateNotificationId);
  }

  void SetShouldShowNotificationForTest(bool show_notification) {
    FirmwareUpdateManager::Get()->set_should_show_notification_for_test(
        show_notification);
  }

  void SimulateFetchingUpdates() {
    FirmwareUpdateManager::Get()->RequestAllUpdates(
        FirmwareUpdateManager::Source::kStartup);
  }

  raw_ptr<FwupdClient, DanglingUntriaged> dbus_client_ = nullptr;
  NetworkHandlerTestHelper network_handler_test_helper_;
  TestingPrefServiceSimple profile_prefs_;
  TestingPrefServiceSimple local_state_;
  std::unique_ptr<FirmwareUpdateManager> firmware_update_manager_;
  std::unique_ptr<FirmwareUpdateNotificationController>
      firmware_update_notification_controller_;
};

TEST_F(FirmwareUpdateStartupNotificationTest,
       StartupNotificationShownRegularUser) {
  // Notification should be shown at login.
  SimulateUserLogin("user1@email.com");
  InitializeNotificationController();
  SimulateFetchingUpdates();
  EXPECT_TRUE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateStartupNotificationTest,
       StartupNotificationShownGuestUser) {
  // Notification should not be shown at login if the user is a guest.
  SimulateUserLogin("user1@email.com", user_manager::UserType::kGuest);
  InitializeNotificationController();
  SimulateFetchingUpdates();
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateStartupNotificationTest, StartupNotificationShownKiosk) {
  // Notification should not be shown at login if the user is in kiosk mode.
  SimulateUserLogin("user1@email.com", user_manager::UserType::kKioskApp);
  InitializeNotificationController();
  SimulateFetchingUpdates();
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}

TEST_F(FirmwareUpdateStartupNotificationTest,
       StartupNotificationShownKioskPWA) {
  // Notification should not be shown at login if the user is in kiosk mode.
  SimulateUserLogin("user1@email.com", user_manager::UserType::kWebKioskApp);
  InitializeNotificationController();
  SimulateFetchingUpdates();
  EXPECT_FALSE(message_center()->FindVisibleNotificationById(
      kFirmwareUpdateNotificationId));
}
}  // namespace ash
