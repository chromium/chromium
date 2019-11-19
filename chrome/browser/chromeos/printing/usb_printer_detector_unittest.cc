// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/usb_printer_detector.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/device/public/cpp/test/fake_usb_device_info.h"
#include "services/device/public/cpp/test/fake_usb_device_manager.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
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
    mojo::PendingRemote<device::mojom::UsbDeviceManager> manager;
    usb_manager_.AddReceiver(manager.InitWithNewPipeAndPassReceiver());

    detector_ = UsbPrinterDetector::CreateForTesting(std::move(manager));
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

}  // namespace
}  // namespace chromeos
