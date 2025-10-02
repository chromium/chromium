// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/managed_printer_translator.h"

#include <optional>

#include "base/test/protobuf_matchers.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_configuration.pb.h"
#include "chrome/browser/ash/printing/enterprise/print_job_options.pb.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::IsEmpty;
using Dict = ::base::Value::Dict;

namespace chromeos {

namespace {

ManagedPrinterConfiguration ValidManagedPrinter() {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_uri("ipp://localhost:8000/ipp/print");
  managed_printer.mutable_ppd_resource()->set_autoconf(true);
  return managed_printer;
}

TEST(ManagedPrinterConfigFromDict, EmptyDict) {
  auto managed_printer = ManagedPrinterConfigFromDict(base::Value::Dict());
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer,
              EqualsProto(ManagedPrinterConfiguration::default_instance()));
}

TEST(ManagedPrinterConfigFromDict, DictWithMultiplePpdResources) {
  auto printer_dict = base::Value::Dict().Set(
      "ppd_resource", base::Value::Dict()
                          .Set("autoconf", true)
                          .Set("user_supplied_ppd_uri", "a")
                          .Set("effective_model", "b"));
  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);
  EXPECT_EQ(managed_printer, std::nullopt);
}

TEST(ManagedPrinterConfigFromDict, DictWithMakeAndModelPpdResource) {
  auto printer_dict =
      base::Value::Dict()
          .Set("guid", "a")
          .Set("display_name", "b")
          .Set("description", "c")
          .Set("uri", "d")
          .Set("ppd_resource", base::Value::Dict().Set("effective_model", "e"));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ManagedPrinterConfiguration expected;
  expected.set_guid("a");
  expected.set_display_name("b");
  expected.set_description("c");
  expected.set_uri("d");
  expected.mutable_ppd_resource()->set_effective_model("e");
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer, EqualsProto(expected));
}

TEST(ManagedPrinterConfigFromDict, DictWithAutoconfPpdResource) {
  auto printer_dict =
      base::Value::Dict()
          .Set("guid", "a")
          .Set("display_name", "b")
          .Set("description", "c")
          .Set("uri", "d")
          .Set("ppd_resource", base::Value::Dict().Set("autoconf", false));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ManagedPrinterConfiguration expected;
  expected.set_guid("a");
  expected.set_display_name("b");
  expected.set_description("c");
  expected.set_uri("d");
  expected.mutable_ppd_resource()->set_autoconf(false);
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer, EqualsProto(expected));
}

TEST(ManagedPrinterConfigFromDict, DictWithUserSuppliedPpdUriPpdResource) {
  auto printer_dict =
      base::Value::Dict()
          .Set("guid", "a")
          .Set("display_name", "b")
          .Set("description", "c")
          .Set("uri", "d")
          .Set("ppd_resource",
               base::Value::Dict().Set("user_supplied_ppd_uri", "e"));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ManagedPrinterConfiguration expected;
  expected.set_guid("a");
  expected.set_display_name("b");
  expected.set_description("c");
  expected.set_uri("d");
  expected.mutable_ppd_resource()->set_user_supplied_ppd_uri("e");
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer, EqualsProto(expected));
}

TEST(ManagedPrinterConfigFromDict, DictWithPrintJobOptions) {
  auto printer_dict = base::Value::Dict().Set(
      "print_job_options",
      base::Value::Dict().Set("color",
                              base::Value::Dict().Set("default_value", true)));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ManagedPrinterConfiguration expected;
  expected.mutable_print_job_options()->mutable_color()->set_default_value(
      true);
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer, EqualsProto(expected));
}

TEST(ManagedPrinterConfigFromDict, DictWithUsbDeviceId) {
  auto printer_dict = Dict().Set(
      "usb_device_id", Dict()
          .Set("vendor_id", 123)
          .Set("product_id", 456)
          .Set("usb_protocol", 1));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ManagedPrinterConfiguration expected;
  expected.mutable_usb_device_id()->set_vendor_id(123);
  expected.mutable_usb_device_id()->set_product_id(456);
  expected.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_LEGACY_USB);
  ASSERT_TRUE(managed_printer.has_value());
  EXPECT_THAT(*managed_printer, EqualsProto(expected));
}

TEST(ManagedPrinterConfigFromDict, DictWithInvalidUsbDeviceId_OutOfRange) {
  auto printer_dict = Dict().Set(
      "usb_device_id", Dict()
          .Set("vendor_id", 65536)
          .Set("product_id", 1)
          .Set("usb_protocol", 1));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ASSERT_FALSE(managed_printer.has_value());
}

TEST(ManagedPrinterConfigFromDict, DictWithInvalidUsbDeviceId_MissingVendorId) {
  auto printer_dict = Dict().Set(
      "usb_device_id", Dict()
          .Set("product_id", 1)
          .Set("usb_protocol", 1));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ASSERT_FALSE(managed_printer.has_value());
}

TEST(ManagedPrinterConfigFromDict, DictWithBothUriAndUsbDeviceId) {
  auto printer_dict = Dict()
      .Set("uri", "d")
      .Set("usb_device_id", Dict()
          .Set("vendor_id", 123)
          .Set("product_id", 456)
          .Set("usb_protocol", 1));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ASSERT_FALSE(managed_printer.has_value());
}

TEST(ManagedPrinterConfigFromDict, DictWithInvalidUsbDeviceId_MissingProtocol) {
  auto printer_dict = Dict()
      .Set("usb_device_id", Dict()
          .Set("vendor_id", 123)
          .Set("product_id", 456));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ASSERT_FALSE(managed_printer.has_value());
}

TEST(ManagedPrinterConfigFromDict, DictWithInvalidUsbDeviceId_InvalidProtocol) {
  auto printer_dict = Dict()
      .Set("usb_device_id", Dict()
          .Set("vendor_id", 123)
          .Set("product_id", 456)
          .Set("usb_protocol", 0));

  auto managed_printer = ManagedPrinterConfigFromDict(printer_dict);

  ASSERT_FALSE(managed_printer.has_value());
}

TEST(PrinterFromManagedPrinterConfig, MissingGuid) {
  auto managed_printer = ValidManagedPrinter();
  managed_printer.clear_guid();
  EXPECT_EQ(PrinterFromManagedPrinterConfig(managed_printer), std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, DisplayName) {
  auto managed_printer = ValidManagedPrinter();
  managed_printer.clear_display_name();
  EXPECT_EQ(PrinterFromManagedPrinterConfig(managed_printer), std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, WithoutUri) {
  auto managed_printer = ValidManagedPrinter();
  managed_printer.clear_uri();
  EXPECT_EQ(PrinterFromManagedPrinterConfig(managed_printer), std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, WithBadUri) {
  auto managed_printer = ValidManagedPrinter();
  managed_printer.set_uri("invalid-uri");
  EXPECT_EQ(PrinterFromManagedPrinterConfig(managed_printer), std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, WithInvalidPpdResource) {
  auto printer_with_no_ppd_resource = ValidManagedPrinter();
  printer_with_no_ppd_resource.clear_ppd_resource();
  auto printer_with_autoconf_false = ValidManagedPrinter();
  printer_with_autoconf_false.mutable_ppd_resource()->set_autoconf(false);
  auto printer_with_bad_user_supplied_ppd_uri = ValidManagedPrinter();
  printer_with_bad_user_supplied_ppd_uri.mutable_ppd_resource()
      ->set_user_supplied_ppd_uri("ftp://scheme-not-allowed");

  EXPECT_EQ(PrinterFromManagedPrinterConfig(printer_with_autoconf_false),
            std::nullopt);
  EXPECT_EQ(PrinterFromManagedPrinterConfig(printer_with_no_ppd_resource),
            std::nullopt);
  EXPECT_EQ(
      PrinterFromManagedPrinterConfig(printer_with_bad_user_supplied_ppd_uri),
      std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, BasicProperties) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->id(), "id");
  EXPECT_EQ(printer->display_name(), "name");
  EXPECT_EQ(printer->uri().GetNormalized(), "ipp://localhost:8000/ipp/print");
}

TEST(PrinterFromManagedPrinterConfig, WithAutoconf) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_ppd_resource()->set_autoconf(true);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_THAT(printer->ppd_reference().effective_make_and_model, IsEmpty());
  EXPECT_THAT(printer->ppd_reference().user_supplied_ppd_url, IsEmpty());
  EXPECT_TRUE(printer->ppd_reference().autoconf);
}

TEST(PrinterFromManagedPrinterConfig, WithModel) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_ppd_resource()->set_autoconf(false);
  managed_printer.mutable_ppd_resource()->set_effective_model("model");

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_THAT(printer->ppd_reference().user_supplied_ppd_url, IsEmpty());
  EXPECT_EQ(printer->ppd_reference().effective_make_and_model, "model");
}

TEST(PrinterFromManagedPrinterConfig, WithUserSuppliedPpdUri) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_ppd_resource()->set_autoconf(false);
  managed_printer.mutable_ppd_resource()->set_user_supplied_ppd_uri(
      "https://ppd-uri");

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_THAT(printer->ppd_reference().effective_make_and_model, IsEmpty());
  EXPECT_EQ(printer->ppd_reference().user_supplied_ppd_url, "https://ppd-uri");
}

TEST(PrinterFromManagedPrinterConfig, WithOptionalFieldsSet) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.set_description("description");

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->description(), "description");
}

TEST(PrinterFromManagedPrinterConfig, WithValidPrintJobOptions) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  PrintJobOptions print_job_options;
  print_job_options.mutable_media_size()->mutable_default_value()->set_height(
      32);
  print_job_options.mutable_media_size()->mutable_default_value()->set_width(
      64);
  print_job_options.mutable_color()->set_default_value(true);
  *managed_printer.mutable_print_job_options() = print_job_options;

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  ASSERT_TRUE(
      printer->print_job_options().media_size.default_value.has_value());
  EXPECT_EQ(printer->print_job_options().media_size.default_value->height, 32);
  EXPECT_EQ(printer->print_job_options().media_size.default_value->width, 64);
}

TEST(PrinterFromManagedPrinterConfig, WithInvalidPrintJobOptions) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  PrintJobOptions print_job_options;
  print_job_options.mutable_media_size()->mutable_default_value()->set_height(
      32);
  *managed_printer.mutable_print_job_options() = print_job_options;

  // Media size default value doesn't have width component, thus the conversion
  // from managed printer should fail.
  EXPECT_EQ(PrinterFromManagedPrinterConfig(managed_printer), std::nullopt);
}

TEST(PrinterFromManagedPrinterConfig, UsbDeviceId) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_usb_device_id()->set_vendor_id(123);
  managed_printer.mutable_usb_device_id()->set_product_id(456);
  managed_printer.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_IPP_USB);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->usb_device_id(), Printer::UsbDeviceId(123, 456));
}

TEST(PrinterFromManagedPrinterConfig, InvalidUsbDeviceId) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_usb_device_id()->set_vendor_id(123);
  managed_printer.mutable_usb_device_id()->set_product_id(-1);
  managed_printer.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_IPP_USB);

  EXPECT_FALSE(PrinterFromManagedPrinterConfig(managed_printer).has_value());
}

TEST(PrinterFromManagedPrinterConfig, UsbProtocol_IPP_USB) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_usb_device_id()->set_vendor_id(123);
  managed_printer.mutable_usb_device_id()->set_product_id(456);
  managed_printer.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_IPP_USB);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->uri().GetNormalized(), "ippusb://007b_01c8/ipp/print");
}

TEST(PrinterFromManagedPrinterConfig, UsbProtocol_LEGACY_USB) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_usb_device_id()->set_vendor_id(123);
  managed_printer.mutable_usb_device_id()->set_product_id(456);
  managed_printer.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_LEGACY_USB);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->uri().GetNormalized(), "usb://007b/01c8?serial");
}

TEST(PrinterFromManagedPrinterConfig, InvalidUsbProtocol) {
  ManagedPrinterConfiguration managed_printer = ValidManagedPrinter();
  managed_printer.mutable_usb_device_id()->set_vendor_id(123);
  managed_printer.mutable_usb_device_id()->set_product_id(456);
  managed_printer.mutable_usb_device_id()->set_usb_protocol(
      ManagedPrinterConfiguration_UsbProtocol::
          ManagedPrinterConfiguration_UsbProtocol_USB_PROTOCOL_UNSPECIFIED);

  EXPECT_FALSE(PrinterFromManagedPrinterConfig(managed_printer).has_value());
}

}  // namespace

}  // namespace chromeos
