// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/services/nearby/public/cpp/nearby_client_uuids.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/bluetooth/bluetooth_power_controller.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/fake_message_center.h"

using testing::NiceMock;
using testing::Return;

namespace ash {
namespace {

const char kTestAdapterName[] = "Chromebook";
const char16_t kTestAdapterName16[] = u"Chromebook";

class TestMessageCenter : public message_center::FakeMessageCenter {
 public:
  TestMessageCenter() = default;

  TestMessageCenter(const TestMessageCenter&) = delete;
  TestMessageCenter& operator=(const TestMessageCenter&) = delete;

  ~TestMessageCenter() override = default;

  void ClickOnNotification(const std::string& id) override {
    message_center::Notification* notification =
        FindVisibleNotificationById(id);
    DCHECK(notification);
    notification->delegate()->Click(absl::nullopt, absl::nullopt);
  }
};

}  // namespace

class BluetoothNotificationControllerTest : public AshTestBase {
 public:
  BluetoothNotificationControllerTest() = default;

  BluetoothNotificationControllerTest(
      const BluetoothNotificationControllerTest&) = delete;
  BluetoothNotificationControllerTest& operator=(
      const BluetoothNotificationControllerTest&) = delete;

  void SetUp() override {
    AshTestBase::SetUp();

    mock_adapter_ =
        base::MakeRefCounted<NiceMock<device::MockBluetoothAdapter>>();
    ON_CALL(*mock_adapter_, IsPresent()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, IsPowered()).WillByDefault(Return(true));
    ON_CALL(*mock_adapter_, GetName()).WillByDefault(Return(kTestAdapterName));
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);

    notification_controller_ =
        std::make_unique<BluetoothNotificationController>(
            &test_message_center_);
    system_tray_client_ = GetSystemTrayClient();

    bluetooth_device_1_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), 0 /* bluetooth_class */, "name_1", "address_1",
            false /* paired */, false /* connected */);
    bluetooth_device_2_ =
        std::make_unique<NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), 0 /* bluetooth_class */, "name_2", "address_2",
            false /* paired */, false /* connected */);

    toast_manager_ = Shell::Get()->toast_manager();
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

  void VerifyDiscoverableToastVisibility(bool visible) {
    if (visible) {
      ToastOverlay* overlay = GetCurrentOverlay();
      ASSERT_NE(nullptr, overlay);
      EXPECT_EQ(
          l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_DISCOVERABLE,
                                     kTestAdapterName16),
          overlay->GetText());
    } else {
      EXPECT_EQ(nullptr, GetCurrentOverlay());
    }
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
    EXPECT_EQ(std::u16string(), visible_notification->title());
    EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_ASH_STATUS_TRAY_BLUETOOTH_PAIRED,
                                         device->GetNameForDisplay()),
              visible_notification->message());
  }

  // Run the notification controller to simulate showing a toast.
  void ShowDiscoverableToast(
      BluetoothNotificationController* notification_controller) {
    notification_controller->NotifyAdapterDiscoverable();
  }

  void ShowPairedNotification(
      BluetoothNotificationController* notification_controller,
      device::MockBluetoothDevice* bluetooth_device) {
    notification_controller->NotifyPairedDevice(bluetooth_device);
  }

  ToastOverlay* GetCurrentOverlay() {
    return toast_manager_->GetCurrentOverlayForTesting();
  }

  TestMessageCenter test_message_center_;
  scoped_refptr<device::MockBluetoothAdapter> mock_adapter_;
  std::unique_ptr<BluetoothNotificationController> notification_controller_;
  TestSystemTrayClient* system_tray_client_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_2_;
  ToastManagerImpl* toast_manager_ = nullptr;
};

// Legacy test class used to provide additional setup to a subset of the tests
// below that we would still like to run, but cannot be run with the Bluetooth
// revamp feature flag enabled since we no longer show a notification when a
// device becomes paired.
class BluetoothNotificationControllerTestLegacy
    : public BluetoothNotificationControllerTest {
 public:
  BluetoothNotificationControllerTestLegacy() = default;

  BluetoothNotificationControllerTestLegacy(
      const BluetoothNotificationControllerTestLegacy&) = delete;
  BluetoothNotificationControllerTestLegacy& operator=(
      const BluetoothNotificationControllerTestLegacy&) = delete;

  void SetUp() override {
    // These tests should only be run with the kBluetoothRevamp feature flag is
    // disabled, and so we force it off here and ensure that the local state
    // prefs that would have been registered had the feature flag been off are
    // registered.
    if (ash::features::IsBluetoothRevampEnabled()) {
      feature_list_.InitAndDisableFeature(features::kBluetoothRevamp);
      BluetoothPowerController::RegisterLocalStatePrefs(
          local_state()->registry());
    }
    BluetoothNotificationControllerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BluetoothNotificationControllerTest, DiscoverableToast) {
  VerifyDiscoverableToastVisibility(/*visible=*/false);

  ShowDiscoverableToast(notification_controller_.get());

  VerifyDiscoverableToastVisibility(/*visible=*/true);
}

TEST_F(BluetoothNotificationControllerTest,
       DiscoverableToast_NearbyShareEnableHighVisibilityRequestActive) {
  VerifyDiscoverableToastVisibility(/*visible=*/false);

  auto* nearby_share_delegate_ = static_cast<TestNearbyShareDelegate*>(
      Shell::Get()->nearby_share_delegate());
  nearby_share_delegate_->set_is_enable_high_visibility_request_active(true);

  ShowDiscoverableToast(notification_controller_.get());

  VerifyDiscoverableToastVisibility(/*visible=*/false);
}

TEST_F(BluetoothNotificationControllerTest,
       DiscoverableToast_NearbyShareHighVisibilityOn) {
  VerifyDiscoverableToastVisibility(/*visible=*/false);

  auto* nearby_share_delegate_ = static_cast<TestNearbyShareDelegate*>(
      Shell::Get()->nearby_share_delegate());
  nearby_share_delegate_->set_is_high_visibility_on(true);

  ShowDiscoverableToast(notification_controller_.get());

  VerifyDiscoverableToastVisibility(/*visible=*/false);
}

TEST_F(BluetoothNotificationControllerTestLegacy,
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

TEST_F(BluetoothNotificationControllerTestLegacy,
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

TEST_F(BluetoothNotificationControllerTestLegacy,
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

TEST_F(BluetoothNotificationControllerTestLegacy,
       PairedDeviceNotification_SystemDismissesNotification) {
  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());

  VerifyPairedNotificationIsVisible(bluetooth_device_1_.get());

  DismissPairedNotification(bluetooth_device_1_.get(), false /* by_user */);

  VerifyPairedNotificationIsNotVisible(bluetooth_device_1_.get());
  EXPECT_EQ(0, system_tray_client_->show_bluetooth_settings_count());
}

TEST_F(BluetoothNotificationControllerTestLegacy,
       PairedDeviceNotification_DeviceConnectionInitiatedByNearbyClient) {
  VerifyPairedNotificationIsNotVisible(bluetooth_device_1_.get());

  base::flat_set<device::BluetoothUUID> uuid_set;
  uuid_set.insert(nearby::GetNearbyClientUuids()[0]);
  ON_CALL(*bluetooth_device_1_, GetUUIDs()).WillByDefault(Return(uuid_set));

  ShowPairedNotification(notification_controller_.get(),
                         bluetooth_device_1_.get());

  VerifyPairedNotificationIsNotVisible(bluetooth_device_1_.get());
}

}  // namespace ash
