// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/bluetooth/bluetooth_notification_controller.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/test/test_nearby_share_delegate.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
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
    notification->delegate()->Click(std::nullopt, std::nullopt);
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

  void VerifyPairingNotificationVisibility(bool visible) {
    EXPECT_EQ(test_message_center_.FindVisibleNotificationById(
                  BluetoothNotificationController::
                      kBluetoothDevicePairingNotificationId) != nullptr,
              visible);
  }

  // Run the notification controller to simulate showing a toast.
  void ShowDiscoverableToast(
      BluetoothNotificationController* notification_controller) {
    notification_controller->NotifyAdapterDiscoverable();
  }

  void ShowPairingNotification(
      BluetoothNotificationController* notification_controller,
      device::MockBluetoothDevice* mock_device) {
    notification_controller->AuthorizePairing(mock_device);
  }

  void SimulateDevicePaired(
      BluetoothNotificationController* notification_controller,
      device::MockBluetoothDevice* mock_device) {
    ON_CALL(*mock_device, IsPaired()).WillByDefault(Return(true));
    notification_controller->DeviceChanged(mock_adapter_.get(), mock_device);
  }

  void SimulateDeviceBonded(
      BluetoothNotificationController* notification_controller,
      device::MockBluetoothDevice* mock_device) {
    ON_CALL(*mock_device, IsBonded()).WillByDefault(Return(true));
    notification_controller->DeviceChanged(mock_adapter_.get(), mock_device);
  }

  ToastOverlay* GetCurrentOverlay() {
    return toast_manager_->GetCurrentOverlayForTesting();
  }

  TestMessageCenter test_message_center_;
  scoped_refptr<device::MockBluetoothAdapter> mock_adapter_;
  std::unique_ptr<BluetoothNotificationController> notification_controller_;
  raw_ptr<TestSystemTrayClient, DanglingUntriaged> system_tray_client_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_1_;
  std::unique_ptr<device::MockBluetoothDevice> bluetooth_device_2_;
  raw_ptr<ToastManagerImpl, DanglingUntriaged> toast_manager_ = nullptr;
};

TEST_F(BluetoothNotificationControllerTest, DiscoverableToast) {
  VerifyDiscoverableToastVisibility(/*visible=*/false);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);

  ShowDiscoverableToast(notification_controller_.get());

  VerifyDiscoverableToastVisibility(/*visible=*/false);

  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);

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

TEST_F(BluetoothNotificationControllerTest, PairingNotification) {
  VerifyPairingNotificationVisibility(/*visible=*/false);

  ShowPairingNotification(notification_controller_.get(),
                          bluetooth_device_1_.get());
  VerifyPairingNotificationVisibility(/*visible=*/true);

  // Simulate the device being paired. This should not remove the pairing
  // notification.
  SimulateDevicePaired(notification_controller_.get(),
                       bluetooth_device_1_.get());
  VerifyPairingNotificationVisibility(/*visible=*/true);

  // Simulate the device being bonded. This should remove the pairing
  // notification.
  SimulateDeviceBonded(notification_controller_.get(),
                       bluetooth_device_1_.get());
  VerifyPairingNotificationVisibility(/*visible=*/false);
}

}  // namespace ash
