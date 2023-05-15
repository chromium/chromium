// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/pcie_peripheral/ash_usb_detector.h"

#include <memory>

#include "base/timer/mock_timer.h"
#include "base/timer/timer.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"
#include "chromeos/ash/components/peripheral_notification/peripheral_notification_manager.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

// USB device product name.
const char* kProductName_1 = "Google Product A";
const char* kManufacturerName = "Google";

}  // namespace

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

    PciguardClient::InitializeFake();
    PeripheralNotificationManager::Initialize(
        /*is_guest_session=*/false,
        /*is_pcie_tunneling_allowed=*/false);
  }

  void TearDown() override {
    ash_usb_detector_.reset();
    PeripheralNotificationManager::Shutdown();
    PciguardClient::Shutdown();
    BrowserWithTestWindowTest::TearDown();
  }

  void ConnectToDeviceManager() {
    AshUsbDetector::Get()->ConnectToDeviceManager();
  }

  int32_t GetOnDeviceCheckedCount() {
    return ash_usb_detector_->GetOnDeviceCheckedCountForTesting();
  }

  int32_t GetNumRequestForUpdates() {
    return ash_usb_detector_->num_request_for_fetch_updates_for_testing();
  }

  void SetIsTesting() { ash_usb_detector_->SetIsTesting(/*is_testing=*/true); }

  void SetFetchUpdatesTimerForTesting(
      std::unique_ptr<base::RepeatingTimer> timer) {
    ash_usb_detector_->SetFetchUpdatesTimerForTesting(std::move(timer));
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

TEST_F(AshUsbDetectorTest, RepeatRequestUpdates) {
  SetIsTesting();
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  // Set up mock timer.
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  SetFetchUpdatesTimerForTesting(std::move(timer));

  // Add a device.
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      /*vendor_id=*/0, /*product_id=*/1, kManufacturerName, kProductName_1,
      /*serial_number=*/"002");

  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  // Continue the timer to first iteration.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetNumRequestForUpdates());

  // Continue the timer to next iteration.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetNumRequestForUpdates());

  // Continue the timer to next iteration.
  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, GetNumRequestForUpdates());
}

TEST_F(AshUsbDetectorTest, RepeatRequestUpdatesWithInterrupts) {
  SetIsTesting();
  ConnectToDeviceManager();
  base::RunLoop().RunUntilIdle();

  // Set up mock timer.
  auto timer = std::make_unique<base::MockRepeatingTimer>();
  auto* timer_ptr = timer.get();
  SetFetchUpdatesTimerForTesting(std::move(timer));

  // Add a device.
  auto device = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      /*vendor_id=*/0, /*product_id=*/1, kManufacturerName, kProductName_1,
      /*serial_number=*/"002");

  device_manager_.AddDevice(device);
  base::RunLoop().RunUntilIdle();

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, GetNumRequestForUpdates());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2, GetNumRequestForUpdates());

  // Now simulate removing a device. This will reset the repeat counter, expect
  // 3 more repeats.
  device_manager_.RemoveDevice(device);
  base::RunLoop().RunUntilIdle();

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3, GetNumRequestForUpdates());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4, GetNumRequestForUpdates());

  timer_ptr->Fire();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5, GetNumRequestForUpdates());
}

}  // namespace ash
