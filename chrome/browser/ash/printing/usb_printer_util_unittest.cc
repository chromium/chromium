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
using testing::ElementsAre;
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

TEST(UsbPrinterUtilTest, GuessEffectiveMakeAndModelDuplicatedManufacturer) {
  UsbDeviceInfo device_info;
  device_info.manufacturer_name = u"bixolon";
  device_info.product_name = u"bixolon abc-123";

  EXPECT_EQ(GuessEffectiveMakeAndModel(device_info), "bixolon abc-123");
}

TEST(UsbPrinterUtilTest, GuessEffectiveMakeAndModel) {
  UsbDeviceInfo device_info;
  device_info.manufacturer_name = u"bixolon";
  device_info.product_name = u"abc-123";

  EXPECT_EQ(GuessEffectiveMakeAndModel(device_info), "bixolon abc-123");
}

TEST(UsbPrinterUtilTest, MakeDisplayNameBothMissing) {
  std::string make_and_model = MakeDisplayName("", "");
  EXPECT_FALSE(make_and_model.empty());
}

TEST(UsbPrinterUtilTest, MakeDisplayNameBothPresent) {
  std::string make_and_model = MakeDisplayName("TheMake", "TheModel");
  EXPECT_NE(make_and_model.find("TheMake"), std::string::npos);
  EXPECT_NE(make_and_model.find("TheModel"), std::string::npos);
}

TEST(UsbPrinterUtilTest, MakeDisplayNameMakeOnly) {
  std::string make_and_model = MakeDisplayName("TheMake", "");
  EXPECT_NE(make_and_model.find("TheMake"), std::string::npos);
}

TEST(UsbPrinterUtilTest, MakeDisplayNameModelOnly) {
  std::string make_and_model = MakeDisplayName("", "TheModel");
  EXPECT_NE(make_and_model.find("TheModel"), std::string::npos);
}

TEST(UsbPrinterUtilTest, SearchDataNotUpdatedWithoutMake) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";
  device_info.manufacturer_name = u"make";
  device_info.product_name = u"model";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  entry.ppd_search_data.make_and_model.push_back("make model");

  chromeos::UsbPrinterId device_id;
  device_id.set_model("new-model");
  UpdateSearchDataFromDeviceId(device_id, &entry);

  EXPECT_EQ("make model", entry.printer.make_and_model());
  EXPECT_THAT(entry.ppd_search_data.make_and_model, ElementsAre("make model"));
}

TEST(UsbPrinterUtilTest, SearchDataNotUpdatedWithoutModel) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";
  device_info.manufacturer_name = u"make";
  device_info.product_name = u"model";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  entry.ppd_search_data.make_and_model.push_back("make model");

  chromeos::UsbPrinterId device_id;
  device_id.set_make("new-make");
  UpdateSearchDataFromDeviceId(device_id, &entry);

  EXPECT_EQ("make model", entry.printer.make_and_model());
  EXPECT_THAT(entry.ppd_search_data.make_and_model, ElementsAre("make model"));
}

TEST(UsbPrinterUtilTest, SearchDataNotUpdatedIfNamesMatch) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";
  device_info.manufacturer_name = u"make";
  device_info.product_name = u"model";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  entry.ppd_search_data.make_and_model.push_back("make model");

  chromeos::UsbPrinterId device_id;
  device_id.set_make("MAKE");
  device_id.set_model("MODEL");
  UpdateSearchDataFromDeviceId(device_id, &entry);

  EXPECT_EQ("make model", entry.printer.make_and_model());
  EXPECT_THAT(entry.ppd_search_data.make_and_model, ElementsAre("make model"));
}

TEST(UsbPrinterUtilTest, SearchDataReplacedForGenericDescriptors) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";
  device_info.manufacturer_name = u"epson";
  device_info.product_name = u"usb2.0 mfp(hi-speed)";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  entry.ppd_search_data.make_and_model.push_back("epson usb2.0 mfp(hi-speed)");

  chromeos::UsbPrinterId device_id;
  device_id.set_make("MAKE");
  device_id.set_model("MODEL");
  UpdateSearchDataFromDeviceId(device_id, &entry);

  EXPECT_EQ("MAKE MODEL", entry.printer.make_and_model());
  EXPECT_THAT(entry.ppd_search_data.make_and_model, ElementsAre("make model"));
}

TEST(UsbPrinterUtilTest, SearchDataAugmentedForNonGenericDescriptors) {
  UsbDeviceInfo device_info;
  device_info.vendor_id = 1;
  device_info.product_id = 2;
  device_info.serial_number = u"";
  device_info.manufacturer_name = u"make";
  device_info.product_name = u"model";

  PrinterDetector::DetectedPrinter entry;
  EXPECT_TRUE(UsbDeviceToPrinter(device_info, &entry));
  entry.ppd_search_data.make_and_model.push_back("make model");

  chromeos::UsbPrinterId device_id;
  device_id.set_make("REAL-MAKE");
  device_id.set_model("REAL-MODEL");
  UpdateSearchDataFromDeviceId(device_id, &entry);

  EXPECT_EQ("make model", entry.printer.make_and_model());
  EXPECT_THAT(entry.ppd_search_data.make_and_model,
              ElementsAre("make model", "real-make real-model"));
}

}  // namespace
}  // namespace ash
