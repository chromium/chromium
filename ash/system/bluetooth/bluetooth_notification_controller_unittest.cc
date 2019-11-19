// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"

using testing::Return;

namespace ash {
namespace {

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;
  ~TestMessageCenter() override = default;

  void ClickOnNotification(const std::string& id) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    DCHECK(notification);
    notification->delegate()->Click(base::nullopt, base::nullopt);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMessageCenter);
};

}  // namespace

class BluetoothNotificationControllerTest : public AshTestBase {
 public:
  BluetoothNotificationControllerTest() = default;

  void SetUp() override {
    AshTestBase::SetUp();
    notification_controller_ =
        std::make_unique<BluetoothNotificationController>(
            &test_message_center_);
    system_tray_client_ = GetSystemTrayClient();

    bluetooth_device_1_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            nullptr /* adapter */, 0 /* bluetooth_class */, "name_1",
            "address_1", false /* paired */, false /* connected */);
    bluetooth_device_2_ =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            nullptr /* adapter */, 0 /* bluetooth_class */, "name_2",
            "address_2", false /* paired */, false /* connected */);
  }

  void ClickPairedNotification(const device::BluetoothDevice* device) {
    test_message_center_.ClickOnNotification(
        BluetoothNotificationController::GetPairedNotificationId(device));
  }

  void DismissPairedNotification(const device::BluetoothDevice* device,
                                 bool by_user) {
    test_message_center_.RemoveNotification(
        BluetoothNotificationController::GetPairedNotificationId(device),
        by_user);
  }

  void VerifyPairedNotificationIsNotVisible(
      const device::BluetoothDevice* device) {
    EXPECT_FALSE(test_message_center_.FindVisibleNotificationById(
        BluetoothNotificationController::GetPairedNotificationId(device)));
  }

  void VerifyPairedNotificationIsVisible(
      const device::BluetoothDevice* device) {
    message_center::Notification* visible_notification =
        test_message_center_.FindVisibleNotificationById(
            BluetoothNotificationController::GetPairedNotificationId(device));
    EXPECT_TRUE(visible_notification);
    EXPECT_EQ(base::string16(), visible_notification->title());
    EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED,
                                         device->GetNameForDisplay()),
              visible_notification->message());
  }

  // Run the notification controller to simulate showing a notification by
  // adding it to the TestMessageCenter.
  void ShowPairedNotification(
      BluetoothNotificationController* notification_controller,
      device::MockBluetoothDevice* bluetooth_device) {
    notification_controller->NotifyPairedDevice(bluetooth_device);
  }

  TestMessageCenter test_message_center_;
  std::unique_ptr<BluetoothNotificationController> notification_controller_;
  TestSystemTrayClient* system_tray_client_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_2_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothNotificationControllerTest);
};

TEST_F(BluetoothNotificationControllerTest,
       PairedDeviceNotification_TapNotification) {
  // Show the notification to the user.
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());

  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());

  ClickPairedNotification(bluetooth_device_1_.get());

  // The notification shouldn't dismiss after a click.
  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());

  // Check the notification controller tried to open the UI.
  EXPECT_EQ(1, system_tray_client_->show_bluetooth_settings_count());
}

TEST_F(BluetoothNotificationControllerTest,
       PairedDeviceNotification_MultipleNotifications) {
  // Show the notification to the user.
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());
  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());

  // Pairing a new device should create a new notification.
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_2_.get());
  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());
  VerifyPairedNotificationIsVisible(bluetooth_device_2_.get());
}

TEST_F(BluetoothNotificationControllerTest,
       PairedDeviceNotification_UserDismissesNotification) {
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_2_.get());

  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());
  VerifyPairedNotificationIsVisible(bluetooth_device_2_.get());

  // Remove one notification, the other one should still be visible.
  DismissPairedNotification(bluetooth_device_1_.get(), true /* by_user */);

  VerifyPairedNotificationIsNotVisible(bluetooth_device_1_.get());
  VerifyPairedNotificationIsVisible(bluetooth_device_2_.get());

  // The settings UI should not open when closing the notification.
  EXPECT_EQ(0, system_tray_client_->show_bluetooth_settings_count());
}

TEST_F(BluetoothNotificationControllerTest,
       PairedDeviceNotification_SystemDismissesNotification) {
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());

  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());

  DismissPairedNotification(bluetooth_device_1_.get(), false /* by_user */);

  VerifyPairedNotificationIsNotVisible(bluetooth_device_1_.get());
  EXPECT_EQ(0, system_tray_client_->show_bluetooth_settings_count());
}

}  // namespace ash
