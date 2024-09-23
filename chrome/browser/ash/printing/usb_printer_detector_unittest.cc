// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_detector.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/session/session_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/public/cpp/test/mock_session_controller.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr uint8_t kPrinterInterfaceClass = 7;
constexpr uint8_t kPrinterInterfaceSubclass = 1;
constexpr uint8_t kPrinterIppUsbProtocol = 4;

constexpr uint8_t kPrinterUsbProtocol = 0xff;
constexpr uint16_t kUsbVenderId = 0x02ad;
constexpr uint16_t kUsbProductId = 0x138c;
constexpr uint8_t kUsbDeviceClass = 0x09;

scoped_refptr<device::FakeUsbDeviceInfo> CreateFakeUsbPrinter(
    bool is_ipp_supported) {
  std::vector<device::mojom::UsbConfigurationInfoPtr> configs;
  configs.push_back(device::mojom::UsbConfigurationInfo::New());
  configs[0]->interfaces.push_back(device::mojom::UsbInterfaceInfo::New());

  auto alternate = device::mojom::UsbAlternateInterfaceInfo::New();
  alternate->class_code = kPrinterInterfaceClass;
  alternate->subclass_code = kPrinterInterfaceSubclass;
  alternate->protocol_code =
      is_ipp_supported ? kPrinterIppUsbProtocol : kPrinterUsbProtocol;

  configs[0]->interfaces[0]->alternates.push_back(std::move(alternate));
  return base::MakeRefCounted<device::FakeUsbDeviceInfo>(
      kUsbVenderId, kUsbProductId, kUsbDeviceClass, std::move(configs));
}

class UsbPrinterDetectorTest : public testing::Test {
 protected:
  class FakePrinterDetectorClient {
   public:
    FakePrinterDetectorClient() = default;

    void WaitForPrinters() {
      base::RunLoop run_loop;
      done_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }

    void OnPrintersFound(
        const std::vector<PrinterDetector::DetectedPrinter>& printers) {
      if (done_callback_)
        std::move(done_callback_).Run();
    }

   private:
    base::OnceClosure done_callback_;
  };

  UsbPrinterDetectorTest() {
    ON_CALL(this->session_controller_, IsScreenLocked).WillByDefault([this]() {
      return this->screen_locked_;
    });
    ON_CALL(this->session_controller_, AddObserver)
        .WillByDefault([this](ash::SessionObserver* observer) {
          this->observer_ = observer;
        });
    ON_CALL(this->session_controller_, RemoveObserver)
        .WillByDefault([this](ash::SessionObserver* observer) {
          this->observer_ = nullptr;
        });

    mojo::PendingRemote<device::mojom::UsbDeviceManager> manager;
    usb_manager_.AddReceiver(manager.InitWithNewPipeAndPassReceiver());

    detector_ = UsbPrinterDetector::CreateForTesting(std::move(manager),
                                                     &session_controller_);
    detector_->RegisterPrintersFoundCallback(
        base::BindRepeating(&FakePrinterDetectorClient::OnPrintersFound,
                            base::Unretained(&detector_client_)));

    fake_non_printer_ = base::MakeRefCounted<device::FakeUsbDeviceInfo>(
        kUsbVenderId, kUsbProductId);
    fake_printer_ = CreateFakeUsbPrinter(false);
    fake_printer_ipp_ = CreateFakeUsbPrinter(true);

    base::RunLoop().RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  bool screen_locked_ = false;
  raw_ptr<ash::SessionObserver> observer_ = nullptr;
  testing::NiceMock<ash::MockSessionController> session_controller_;
  std::unique_ptr<UsbPrinterDetector> detector_;
  FakePrinterDetectorClient detector_client_;
  device::FakeUsbDeviceManager usb_manager_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_non_printer_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_printer_;
  scoped_refptr<device::FakeUsbDeviceInfo> fake_printer_ipp_;
};

// Test GetPrinters().
TEST_F(UsbPrinterDetectorTest, GetPrinters) {
  EXPECT_EQ(0u, detector_->GetPrinters().size());

  usb_manager_.AddDevice(fake_non_printer_);
  usb_manager_.AddDevice(fake_printer_);
  usb_manager_.AddDevice(fake_printer_ipp_);

  detector_client_.WaitForPrinters();
  EXPECT_EQ(1u, detector_->GetPrinters().size());

  detector_client_.WaitForPrinters();
  EXPECT_EQ(2u, detector_->GetPrinters().size());
}

// Test OnPrintersFound callback on deviceAdded/Removed.
TEST_F(UsbPrinterDetectorTest, OnPrintersFoundCallback) {
  EXPECT_EQ(0u, detector_->GetPrinters().size());

  usb_manager_.AddDevice(fake_non_printer_);
  usb_manager_.AddDevice(fake_printer_);
  usb_manager_.AddDevice(fake_printer_ipp_);

  detector_client_.WaitForPrinters();
  EXPECT_EQ(1u, detector_->GetPrinters().size());

  detector_client_.WaitForPrinters();
  EXPECT_EQ(2u, detector_->GetPrinters().size());

  usb_manager_.RemoveDevice(fake_non_printer_);
  usb_manager_.RemoveDevice(fake_printer_);
  usb_manager_.RemoveDevice(fake_printer_ipp_);

  detector_client_.WaitForPrinters();
  EXPECT_EQ(1u, detector_->GetPrinters().size());

  detector_client_.WaitForPrinters();
  EXPECT_EQ(0u, detector_->GetPrinters().size());
}

// New printers are not announced as long as the screen is locked.
TEST_F(UsbPrinterDetectorTest, NewPrintersHoldWhenTheScreenIsLocked) {
  screen_locked_ = true;

  usb_manager_.AddDevice(fake_printer_);
  usb_manager_.AddDevice(fake_printer_ipp_);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, detector_->GetPrinters().size());

  ASSERT_NE(observer_, nullptr);
  observer_->OnLockStateChanged(screen_locked_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, detector_->GetPrinters().size());

  screen_locked_ = false;
  observer_->OnLockStateChanged(screen_locked_);

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(2u, detector_->GetPrinters().size());
}

}  // namespace
}  // namespace ash
