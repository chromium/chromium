// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"

#include "ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/dbus/pciguard/pciguard_client.h"
#include "chromeos/dbus/typecd/typecd_client.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
// USB device product name.
const char* kProductName_1 = "Google Product A";
const char* kManufacturerName = "Google";
}  // namespace

namespace ash {

class AshUsbDetectorTest : public BrowserWithTestWindowTest {
 public:
  AshUsbDetectorTest() = default;
  AshUsbDetectorTest(const AshUsbDetectorTest&) = delete;
  AshUsbDetectorTest& operator=(const AshUsbDetectorTest&) = delete;
  ~AshUsbDetectorTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    ash_usb_detector_ = std::make_unique<AshUsbDetector>();

    // Set a fake USB device manager before ConnectToDeviceManager().
    mojo::PendingRemote<device::mojom::UsbDeviceManager> device_manager;
    device_manager_.AddReceiver(
        device_manager.InitWithNewPipeAndPassReceiver());
    AshUsbDetector::Get()->SetDeviceManagerForTesting(
        std::move(device_manager));

    chromeos::TypecdClient::InitializeFake();
    chromeos::PciguardClient::InitializeFake();
    PeripheralNotificationManager::Initialize(
        /*is_guest_session=*/false,
        /*is_pcie_tunneling_allowed=*/false);
  }

  void TearDown() override {
    BrowserWithTestWindowTest::TearDown();
    ash_usb_detector_.reset();
  }

  void ConnectToDeviceManager() {
    AshUsbDetector::Get()->ConnectToDeviceManager();
  }

  int32_t GetOnDeviceCheckedCount() {
    return ash_usb_detector_->GetOnDeviceCheckedCountForTesting();
  }

  device::FakeUsbDeviceManager device_manager_;
  std::unique_ptr<AshUsbDetector> ash_usb_detector_;
};

TEST_F(AshUsbDetectorTest, AddOneDevice) {
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      /*vendor_id=*/0, /*product_id=*/1, kManufacturerName, kProductName_1,
      /*serial_number=*/"002");

  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, GetOnDeviceCheckedCount());
}

}  // namespace ash
