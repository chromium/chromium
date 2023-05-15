// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/usb_printer_util.h"

#include "services/device/public/mojom/usb_device.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using device::mojom::UsbDeviceInfo;
using testing::HasSubstr;

TEST(UsbPrinterUtilTest, UsbDeviceToPrinterWithValidSerialNumber) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"12345";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  EXPECT_THAT(entry.printer.uri().GetNormalized(), HasSubstr("?serial=12345"));
}

TEST(UsbPrinterUtilTest, UsbDeviceToPrinterWithNoSerialNumber) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  // If the device_info does not specify a serial number, the URI should get
  // created with the character '?'.
  EXPECT_THAT(entry.printer.uri().GetNormalized(), HasSubstr("?serial=?"));
}

TEST(UsbPrinterUtilTest, UsbDeviceToPrinterWithEmptySerialNumber) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  // If the device_info specifies an empty serial number, the URI should get
  // created with the character '?'.
  EXPECT_THAT(entry.printer.uri().GetNormalized(), HasSubstr("?serial=?"));
}

}  // namespace
}  // namespace ash
