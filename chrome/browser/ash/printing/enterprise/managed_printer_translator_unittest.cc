// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/managed_printer_translator.h"

#include <optional>

#include "base/test/protobuf_matchers.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/managed_printer_configuration.pb.h"
#include "chromeos/printing/printer_configuration.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::test::EqualsProto;
using ::testing::IsEmpty;

namespace chromeos {

namespace {

ManagedPrinterConfiguration ValidManagedPrinter() {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_description("description");
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

TEST(PrinterFromManagedPrinterConfig, WithAutoconf) {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_uri("ipp://host:1234/ipp/print");
  managed_printer.mutable_ppd_resource()->set_autoconf(true);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->id(), "id");
  EXPECT_EQ(printer->display_name(), "name");
  EXPECT_EQ(printer->uri().GetNormalized(), "ipp://host:1234/ipp/print");
  EXPECT_THAT(printer->ppd_reference().effective_make_and_model, IsEmpty());
  EXPECT_THAT(printer->ppd_reference().user_supplied_ppd_url, IsEmpty());
  EXPECT_TRUE(printer->ppd_reference().autoconf);
}

TEST(PrinterFromManagedPrinterConfig, WithModel) {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_uri("ipp://host:1234/ipp/print");
  managed_printer.mutable_ppd_resource()->set_effective_model("model");

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->id(), "id");
  EXPECT_EQ(printer->display_name(), "name");
  EXPECT_EQ(printer->uri().GetNormalized(), "ipp://host:1234/ipp/print");
  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_THAT(printer->ppd_reference().user_supplied_ppd_url, IsEmpty());
  EXPECT_EQ(printer->ppd_reference().effective_make_and_model, "model");
}

TEST(PrinterFromManagedPrinterConfig, WithUserSuppliedPpdUri) {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_uri("ipp://host:1234/ipp/print");
  managed_printer.mutable_ppd_resource()->set_user_supplied_ppd_uri(
      "https://ppd-uri");

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->id(), "id");
  EXPECT_EQ(printer->display_name(), "name");
  EXPECT_EQ(printer->uri().GetNormalized(), "ipp://host:1234/ipp/print");
  EXPECT_FALSE(printer->ppd_reference().autoconf);
  EXPECT_THAT(printer->ppd_reference().effective_make_and_model, IsEmpty());
  EXPECT_EQ(printer->ppd_reference().user_supplied_ppd_url, "https://ppd-uri");
}

TEST(PrinterFromManagedPrinterConfig, WithOptionalFieldsSet) {
  ManagedPrinterConfiguration managed_printer;
  managed_printer.set_guid("id");
  managed_printer.set_display_name("name");
  managed_printer.set_description("description");
  managed_printer.set_uri("ipp://host:1234/ipp/print");
  managed_printer.mutable_ppd_resource()->set_autoconf(true);

  std::optional<Printer> printer =
      PrinterFromManagedPrinterConfig(managed_printer);

  ASSERT_TRUE(printer.has_value());
  EXPECT_EQ(printer->id(), "id");
  EXPECT_EQ(printer->display_name(), "name");
  EXPECT_EQ(printer->description(), "description");
  EXPECT_EQ(printer->uri().GetNormalized(), "ipp://host:1234/ipp/print");
  EXPECT_TRUE(printer->ppd_reference().autoconf);
  EXPECT_THAT(printer->ppd_reference().user_supplied_ppd_url, IsEmpty());
  EXPECT_THAT(printer->ppd_reference().effective_make_and_model, IsEmpty());
}

}  // namespace

}  // namespace chromeos
