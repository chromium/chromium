// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"

#include "base/containers/contains.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

TEST(DocumentScanAshTypeConvertersTest, OperationResult) {
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNKNOWN),
            crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_SUCCESS),
            crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNSUPPORTED),
            crosapi::mojom::ScannerOperationResult::kUnsupported);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_CANCELLED),
            crosapi::mojom::ScannerOperationResult::kCancelled);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_DEVICE_BUSY),
            crosapi::mojom::ScannerOperationResult::kDeviceBusy);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_INVALID),
            crosapi::mojom::ScannerOperationResult::kInvalid);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_WRONG_TYPE),
            crosapi::mojom::ScannerOperationResult::kWrongType);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_EOF),
            crosapi::mojom::ScannerOperationResult::kEndOfData);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ADF_JAMMED),
            crosapi::mojom::ScannerOperationResult::kAdfJammed);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ADF_EMPTY),
            crosapi::mojom::ScannerOperationResult::kAdfEmpty);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_COVER_OPEN),
            crosapi::mojom::ScannerOperationResult::kCoverOpen);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_IO_ERROR),
            crosapi::mojom::ScannerOperationResult::kIoError);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_ACCESS_DENIED),
            crosapi::mojom::ScannerOperationResult::kAccessDenied);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_NO_MEMORY),
            crosapi::mojom::ScannerOperationResult::kNoMemory);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_UNREACHABLE),
            crosapi::mojom::ScannerOperationResult::kDeviceUnreachable);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_MISSING),
            crosapi::mojom::ScannerOperationResult::kDeviceMissing);
  EXPECT_EQ(ConvertTo<crosapi::mojom::ScannerOperationResult>(
                lorgnette::OPERATION_RESULT_INTERNAL_ERROR),
            crosapi::mojom::ScannerOperationResult::kInternalError);
}

TEST(DocumentScanAshTypeConvertersTest, OptionType) {
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_UNKNOWN),
            crosapi::mojom::OptionType::kUnknown);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_BOOL),
            crosapi::mojom::OptionType::kBool);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_INT),
            crosapi::mojom::OptionType::kInt);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_FIXED),
            crosapi::mojom::OptionType::kFixed);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_STRING),
            crosapi::mojom::OptionType::kString);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_BUTTON),
            crosapi::mojom::OptionType::kButton);
  EXPECT_EQ(ConvertForTesting(lorgnette::TYPE_GROUP),
            crosapi::mojom::OptionType::kGroup);
}

TEST(DocumentScanAshTypeConvertersTest, OptionUnit) {
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_NONE),
            crosapi::mojom::OptionUnit::kUnitless);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_PIXEL),
            crosapi::mojom::OptionUnit::kPixel);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_BIT),
            crosapi::mojom::OptionUnit::kBit);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_MM),
            crosapi::mojom::OptionUnit::kMm);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_DPI),
            crosapi::mojom::OptionUnit::kDpi);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_PERCENT),
            crosapi::mojom::OptionUnit::kPercent);
  EXPECT_EQ(ConvertForTesting(lorgnette::UNIT_MICROSECOND),
            crosapi::mojom::OptionUnit::kMicrosecond);
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraintType) {
  EXPECT_EQ(ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_NONE),
            crosapi::mojom::OptionConstraintType::kNone);
  EXPECT_EQ(
      ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE),
      crosapi::mojom::OptionConstraintType::kIntRange);
  EXPECT_EQ(
      ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE),
      crosapi::mojom::OptionConstraintType::kFixedRange);
  EXPECT_EQ(ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_INT_LIST),
            crosapi::mojom::OptionConstraintType::kIntList);
  EXPECT_EQ(
      ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST),
      crosapi::mojom::OptionConstraintType::kFixedList);
  EXPECT_EQ(
      ConvertForTesting(lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST),
      crosapi::mojom::OptionConstraintType::kStringList);
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_TypeNone) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_NONE);
  input.mutable_int_range()->set_min(42);
  input.mutable_fixed_range()->set_min(42);
  input.add_valid_int(1);
  input.add_valid_fixed(2.0);
  input.add_valid_string("string");
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kNone);
  EXPECT_TRUE(output->restriction.is_null());
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_IntRangeWithoutRange) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kIntRange);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_int_range());
  EXPECT_EQ(output->restriction->get_int_range()->min, 0);
  EXPECT_EQ(output->restriction->get_int_range()->max, 0);
  EXPECT_EQ(output->restriction->get_int_range()->quant, 0);
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_IntRangeWithRange) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE);
  input.mutable_int_range()->set_min(1);
  input.mutable_int_range()->set_max(10);
  input.mutable_int_range()->set_quant(2);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kIntRange);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_int_range());
  EXPECT_EQ(output->restriction->get_int_range()->min, 1);
  EXPECT_EQ(output->restriction->get_int_range()->max, 10);
  EXPECT_EQ(output->restriction->get_int_range()->quant, 2);
}

TEST(DocumentScanAshTypeConvertersTest,
     OptionConstraint_FixedRangeWithoutRange) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kFixedRange);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_fixed_range());
  EXPECT_EQ(output->restriction->get_fixed_range()->min, 0.0);
  EXPECT_EQ(output->restriction->get_fixed_range()->max, 0.0);
  EXPECT_EQ(output->restriction->get_fixed_range()->quant, 0.0);
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_FixedRangeWithRange) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE);
  input.mutable_fixed_range()->set_min(1.0);
  input.mutable_fixed_range()->set_max(10.0);
  input.mutable_fixed_range()->set_quant(0.5);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kFixedRange);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_fixed_range());
  EXPECT_EQ(output->restriction->get_fixed_range()->min, 1.0);
  EXPECT_EQ(output->restriction->get_fixed_range()->max, 10.0);
  EXPECT_EQ(output->restriction->get_fixed_range()->quant, 0.5);
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_IntListWithoutList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_LIST);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kIntList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_int());
  EXPECT_TRUE(output->restriction->get_valid_int().empty());
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_IntListWithList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_LIST);
  input.add_valid_int(42);
  input.add_valid_int(43);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kIntList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_int());
  EXPECT_THAT(output->restriction->get_valid_int(), ElementsAre(42, 43));
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_FixedListWithoutList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kFixedList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_fixed());
  EXPECT_TRUE(output->restriction->get_valid_fixed().empty());
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_FixedListWithList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST);
  input.add_valid_fixed(0.25);
  input.add_valid_fixed(0.5);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kFixedList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_fixed());
  EXPECT_THAT(output->restriction->get_valid_fixed(), ElementsAre(0.25, 0.5));
}

TEST(DocumentScanAshTypeConvertersTest,
     OptionConstraint_StringListWithoutList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kStringList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_string());
  EXPECT_TRUE(output->restriction->get_valid_string().empty());
}

TEST(DocumentScanAshTypeConvertersTest, OptionConstraint_StringListWithList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST);
  input.add_valid_string("1");
  input.add_valid_string("2");
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionConstraintType::kStringList);
  ASSERT_FALSE(output->restriction.is_null());
  ASSERT_TRUE(output->restriction->is_valid_string());
  EXPECT_THAT(output->restriction->get_valid_string(), ElementsAre("1", "2"));
}

// Test all the fields that don't require complicated logic so they can be
// omitted from the subsequent tests.
TEST(DocumentScanAshTypeConvertersTest, ScannerOption_BasicFields) {
  lorgnette::ScannerOption input;
  input.set_name("option-name");
  input.set_title("Option title");
  input.set_description("Option Description\nLine 2");
  input.set_option_type(lorgnette::OptionType::TYPE_BUTTON);
  input.set_unit(lorgnette::OptionUnit::UNIT_NONE);
  input.set_detectable(true);
  input.set_auto_settable(true);
  input.set_emulated(true);
  input.set_active(true);
  input.set_advanced(true);

  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->name, "option-name");
  EXPECT_EQ(output->title, "Option title");
  EXPECT_EQ(output->description, "Option Description\nLine 2");
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kButton);
  EXPECT_EQ(output->unit, crosapi::mojom::OptionUnit::kUnitless);
  EXPECT_TRUE(output->value.is_null());
  EXPECT_TRUE(output->constraint.is_null());
  EXPECT_TRUE(output->isDetectable);
  EXPECT_EQ(output->configurability,
            crosapi::mojom::OptionConfigurability::kNotConfigurable);
  EXPECT_TRUE(output->isAutoSettable);
  EXPECT_TRUE(output->isEmulated);
  EXPECT_TRUE(output->isActive);
  EXPECT_TRUE(output->isAdvanced);
  EXPECT_FALSE(output->isInternal);
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_ConfigurabilitySoftware) {
  lorgnette::ScannerOption input;
  input.set_sw_settable(true);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->configurability,
            crosapi::mojom::OptionConfigurability::kSoftwareConfigurable);
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_ConfigurabilityHardware) {
  lorgnette::ScannerOption input;
  input.set_hw_settable(true);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->configurability,
            crosapi::mojom::OptionConfigurability::kHardwareConfigurable);
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_BoolWithoutBoolValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_BOOL);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kBool);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_BoolWithBoolValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_BOOL);
  input.set_bool_value(true);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kBool);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_bool_value());
  EXPECT_EQ(output->value->get_bool_value(), true);
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_IntWithoutIntValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_INT);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kInt);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_IntWithOneIntValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_INT);
  input.mutable_int_value()->add_value(1);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kInt);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_int_value());
  EXPECT_EQ(output->value->get_int_value(), 1);
}

TEST(DocumentScanAshTypeConvertersTest,
     ScannerOption_IntWithMultipleIntValues) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_INT);
  input.mutable_int_value()->add_value(5);
  input.mutable_int_value()->add_value(4);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kInt);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_int_list());
  EXPECT_THAT(output->value->get_int_list(), ElementsAre(5, 4));
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_FixedWithoutFixedValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_FIXED);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kFixed);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_FixedWithOneFixedValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_FIXED);
  input.mutable_fixed_value()->add_value(3.25);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kFixed);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_fixed_value());
  EXPECT_EQ(output->value->get_fixed_value(), 3.25);
}

TEST(DocumentScanAshTypeConvertersTest,
     ScannerOption_FixedWithMultipleFixedValues) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_FIXED);
  input.mutable_fixed_value()->add_value(5.0);
  input.mutable_fixed_value()->add_value(4.5);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kFixed);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_fixed_list());
  EXPECT_THAT(output->value->get_fixed_list(), ElementsAre(5.0, 4.5));
}

TEST(DocumentScanAshTypeConvertersTest,
     ScannerOption_StringWithoutStringValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_STRING);
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kString);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanAshTypeConvertersTest,
     ScannerOption_StringWithEmptyStringValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_STRING);
  input.set_string_value("");
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kString);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_string_value());
  EXPECT_EQ(output->value->get_string_value(), "");
}

TEST(DocumentScanAshTypeConvertersTest,
     ScannerOption_StringWithNonEmptyStringValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_STRING);
  input.set_string_value("string");
  auto output = ConvertForTesting(input);
  EXPECT_EQ(output->type, crosapi::mojom::OptionType::kString);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_string_value());
  EXPECT_EQ(output->value->get_string_value(), "string");
}

// Verifies that a constraint is copied in.  Details of the OptionConstraint
// conversion are covered in specific tests above.  The case of a missing
// constraint is covered in the BasicFields test.
TEST(DocumentScanAshTypeConvertersTest, ScannerOption_NonEmptyConstraint) {
  lorgnette::ScannerOption input;
  input.mutable_constraint()->set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST);
  input.mutable_constraint()->add_valid_string("string");
  auto output = ConvertForTesting(input);
  ASSERT_FALSE(output->constraint.is_null());
  EXPECT_EQ(output->constraint->type,
            crosapi::mojom::OptionConstraintType::kStringList);
  ASSERT_FALSE(output->constraint->restriction.is_null());
  ASSERT_TRUE(output->constraint->restriction->is_valid_string());
  EXPECT_THAT(output->constraint->restriction->get_valid_string(),
              ElementsAre("string"));
}

TEST(DocumentScanAshTypeConvertersTest,
     GetScannerListResponse_EmptyObjectSucceeds) {
  lorgnette::ListScannersResponse input;
  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_EQ(output->scanners.size(), 0U);
}

TEST(DocumentScanAshTypeConvertersTest, GetScannerListResponse_UsbScanner) {
  lorgnette::ListScannersResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerInfo* scanner = input.add_scanners();
  scanner->set_name("backend:usb:18d1:505e");
  scanner->set_manufacturer("GoogleTest");
  scanner->set_model("USB Scanner");
  scanner->set_display_name("GoogleTest USB Scanner (USB)");
  scanner->set_device_uuid("13245-67890");
  scanner->set_connection_type(lorgnette::CONNECTION_USB);
  scanner->set_secure(true);
  scanner->add_image_format("image/png");
  scanner->add_image_format("image/jpeg");
  scanner->set_protocol_type("backend");

  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  ASSERT_EQ(output->scanners.size(), 1U);
  crosapi::mojom::ScannerInfoPtr& scanner_out = output->scanners[0];
  EXPECT_EQ(scanner_out->id, "backend:usb:18d1:505e");
  EXPECT_EQ(scanner_out->manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out->model, "USB Scanner");
  EXPECT_EQ(scanner_out->display_name, "GoogleTest USB Scanner (USB)");
  EXPECT_EQ(scanner_out->device_uuid, "13245-67890");
  EXPECT_EQ(scanner_out->connection_type,
            crosapi::mojom::ScannerInfo_ConnectionType::kUsb);
  EXPECT_TRUE(scanner_out->secure);
  EXPECT_THAT(scanner_out->image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out->protocol_type, "backend");
}

TEST(DocumentScanAshTypeConvertersTest, GetScannerListResponse_NetworkScanner) {
  lorgnette::ListScannersResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_NO_MEMORY);
  lorgnette::ScannerInfo* scanner = input.add_scanners();
  scanner->set_name("backend:net:127.0.0.1");
  scanner->set_manufacturer("GoogleTest");
  scanner->set_model("Network Scanner");
  scanner->set_display_name("GoogleTest Network Scanner");
  scanner->set_device_uuid("13245-67890");
  scanner->set_connection_type(lorgnette::CONNECTION_NETWORK);
  scanner->set_secure(false);
  scanner->add_image_format("image/png");
  scanner->add_image_format("image/jpeg");
  scanner->set_protocol_type("backend");

  auto output = crosapi::mojom::GetScannerListResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kNoMemory);
  ASSERT_EQ(output->scanners.size(), 1U);
  crosapi::mojom::ScannerInfoPtr& scanner_out = output->scanners[0];
  EXPECT_EQ(scanner_out->id, "backend:net:127.0.0.1");
  EXPECT_EQ(scanner_out->manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out->model, "Network Scanner");
  EXPECT_EQ(scanner_out->display_name, "GoogleTest Network Scanner");
  EXPECT_EQ(scanner_out->device_uuid, "13245-67890");
  EXPECT_EQ(scanner_out->connection_type,
            crosapi::mojom::ScannerInfo_ConnectionType::kNetwork);
  EXPECT_FALSE(scanner_out->secure);
  EXPECT_THAT(scanner_out->image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out->protocol_type, "backend");
}

TEST(DocumentScanAshTypeConvertersTest,
     OpenScannerResponse_EmptyObjectSucceeds) {
  lorgnette::OpenScannerResponse input;
  auto output = crosapi::mojom::OpenScannerResponse::From(input);
  EXPECT_TRUE(output->scanner_id.empty());
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_FALSE(output->scanner_handle.has_value());
  EXPECT_FALSE(output->options.has_value());
}

// Test that all provided options are copied into the mojom result.  Ensuring
// that option details are converted correctly is covered by specific
// ScannerOption tests above.
TEST(DocumentScanAshTypeConvertersTest,
     OpenScannerResponse_AllOptionsIncluded) {
  lorgnette::OpenScannerResponse input;
  input.mutable_scanner_id()->set_connection_string("scanner:id");
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_config()->mutable_scanner()->set_token("12345");
  (*input.mutable_config()->mutable_options())["option1-name"] = {};
  (*input.mutable_config()->mutable_options())["option2-name"] = {};
  auto output = crosapi::mojom::OpenScannerResponse::From(input);
  EXPECT_EQ(output->scanner_id, "scanner:id");
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  ASSERT_TRUE(output->scanner_handle.has_value());
  EXPECT_EQ(output->scanner_handle.value(), "12345");
  ASSERT_TRUE(output->options.has_value());
  EXPECT_TRUE(base::Contains(output->options.value(), "option1-name"));
  EXPECT_TRUE(base::Contains(output->options.value(), "option2-name"));
}

TEST(DocumentScanAshTypeConvertersTest,
     CloseScannerResponse_EmptyObjectSucceeds) {
  lorgnette::CloseScannerResponse input;
  auto output = crosapi::mojom::CloseScannerResponse::From(input);
  EXPECT_TRUE(output->scanner_handle.empty());
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
}

TEST(DocumentScanAshTypeConvertersTest, CloseScannerResponse_NonEmptyResponse) {
  lorgnette::CloseScannerResponse input;
  input.mutable_scanner()->set_token("55555");
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  auto output = crosapi::mojom::CloseScannerResponse::From(input);
  EXPECT_EQ(output->scanner_handle, "55555");
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
}

TEST(DocumentScanAshTypeConvertersTest, StartPreparedScanResponse_EmptyObject) {
  lorgnette::StartPreparedScanResponse input;
  auto output = crosapi::mojom::StartPreparedScanResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_TRUE(output->scanner_handle.empty());
  EXPECT_FALSE(output->job_handle.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, StartPreparedScanResponse_Success) {
  lorgnette::StartPreparedScanResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_scanner()->set_token("scanner-handle");
  input.mutable_job_handle()->set_token("job-handle");

  auto output = crosapi::mojom::StartPreparedScanResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(output->scanner_handle, "scanner-handle");
  ASSERT_TRUE(output->job_handle.has_value());
  EXPECT_EQ(output->job_handle.value(), "job-handle");
}

TEST(DocumentScanAshTypeConvertersTest, ReadScanDataResponse_EmptyObject) {
  lorgnette::ReadScanDataResponse input;
  auto output = crosapi::mojom::ReadScanDataResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_TRUE(output->job_handle.empty());
  EXPECT_FALSE(output->data.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, ReadScanDataResponse_Success) {
  lorgnette::ReadScanDataResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_job_handle()->set_token("job-handle");
  input.set_data("data");
  input.set_estimated_completion(23);

  auto output = crosapi::mojom::ReadScanDataResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(output->job_handle, "job-handle");
  ASSERT_TRUE(output->data.has_value());
  EXPECT_THAT(output->data.value(), ElementsAre('d', 'a', 't', 'a'));
  EXPECT_EQ(output->estimated_completion, 23U);
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_EmptyObject) {
  auto input = crosapi::mojom::OptionSetting::New();
  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_TRUE(output->name().empty());
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_UNKNOWN);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessBool) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kBool;
  input->value = crosapi::mojom::OptionValue::NewBoolValue(true);

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_BOOL);
  EXPECT_TRUE(output->has_bool_value());
  EXPECT_TRUE(output->bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessInt) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kInt;
  input->value = crosapi::mojom::OptionValue::NewIntValue(10);

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_TRUE(output->has_int_value());
  EXPECT_THAT(output->int_value().value(), ElementsAre(10));
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessIntList) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kInt;
  input->value = crosapi::mojom::OptionValue::NewIntList({10, 20});

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_TRUE(output->has_int_value());
  EXPECT_THAT(output->int_value().value(), ElementsAre(10, 20));
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessFixed) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kFixed;
  input->value = crosapi::mojom::OptionValue::NewFixedValue(10.5);

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_FIXED);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_TRUE(output->has_fixed_value());
  EXPECT_THAT(output->fixed_value().value(), ElementsAre(10.5));
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessFixedList) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kFixed;
  input->value = crosapi::mojom::OptionValue::NewFixedList({10.4, 20.6});

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_FIXED);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_TRUE(output->has_fixed_value());
  EXPECT_THAT(output->fixed_value().value(), ElementsAre(10.4, 20.6));
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessString) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kString;
  input->value = crosapi::mojom::OptionValue::NewStringValue("option-value");

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_STRING);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_TRUE(output->has_string_value());
  EXPECT_EQ(output->string_value(), "option-value");
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_BoolNoValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kBool;
  // Don't set a value for this option.

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_BOOL);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_BoolWrongValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kBool;
  // Set an incorrect value for this option.
  input->value = crosapi::mojom::OptionValue::NewStringValue("bad-value");

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  EXPECT_FALSE(output.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_IntNoValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kInt;
  // Don't set a value for this option.

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_IntWrongValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kInt;
  // Set an incorrect value for this option.
  input->value = crosapi::mojom::OptionValue::NewStringValue("bad-value");

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  EXPECT_FALSE(output.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_FixedNoValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kFixed;
  // Don't set a value for this option.

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_FIXED);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_FixedWrongValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kFixed;
  // Set an incorrect value for this option.
  input->value = crosapi::mojom::OptionValue::NewStringValue("bad-value");

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  EXPECT_FALSE(output.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_StringNoValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kString;
  // Don't set a value for this option.

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_STRING);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_StringWrongValue) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kString;
  // Set an incorrect value for this option.
  input->value = crosapi::mojom::OptionValue::NewBoolValue(false);

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  EXPECT_FALSE(output.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessUnknown) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kUnknown;
  // Even if a value is set on the input, it won't be set on the output since
  // the type is unknown.
  input->value = crosapi::mojom::OptionValue::NewStringValue("not-used");

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_UNKNOWN);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessButton) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kButton;
  // Even if a value is set on the input, it won't be set on the output since
  // the type is button.
  input->value = crosapi::mojom::OptionValue::NewIntValue(10);

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_BUTTON);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, ScannerOption_SuccessGroup) {
  auto input = crosapi::mojom::OptionSetting::New();
  input->name = "option-name";
  input->type = crosapi::mojom::OptionType::kGroup;
  // Even if a value is set on the input, it won't be set on the output since
  // the type is group.
  input->value = crosapi::mojom::OptionValue::NewFixedList({10.4, 20.6});

  auto output = input.To<std::optional<lorgnette::ScannerOption>>();

  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option-name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_GROUP);
  EXPECT_FALSE(output->has_bool_value());
  EXPECT_FALSE(output->has_int_value());
  EXPECT_FALSE(output->has_fixed_value());
  EXPECT_FALSE(output->has_string_value());
}

TEST(DocumentScanAshTypeConvertersTest, SetOptionsResponse_EmptyObject) {
  lorgnette::SetOptionsResponse input;
  auto output = crosapi::mojom::SetOptionsResponse::From(input);
  EXPECT_TRUE(output->scanner_handle.empty());
  EXPECT_TRUE(output->results.empty());
  ASSERT_TRUE(output->options.has_value());
  EXPECT_TRUE(output->options->empty());
}

TEST(DocumentScanAshTypeConvertersTest, SetOptionsResponse_Success) {
  lorgnette::SetOptionsResponse input;
  input.mutable_scanner()->set_token("scanner-handle");
  // For this input, we can just use a single status since other tests will test
  // this exhaustively.  This test just needs to make sure the status is passed
  // back.
  (*input.mutable_results())["result"] = lorgnette::OPERATION_RESULT_SUCCESS;
  // Likewise, we just need a single option with a single attribute populated.
  // This doesn't need to exhaustively test ScannerOption - there are other
  // tests that will do that.
  lorgnette::ScannerConfig config;
  lorgnette::ScannerOption option;
  option.set_name("option-name");
  (*config.mutable_options())["option-name"] = std::move(option);
  *input.mutable_config() = config;

  auto output = crosapi::mojom::SetOptionsResponse::From(input);
  EXPECT_EQ(output->scanner_handle, "scanner-handle");
  ASSERT_EQ(output->results.size(), 1U);
  EXPECT_EQ(output->results[0]->name, "result");
  EXPECT_EQ(output->results[0]->result,
            crosapi::mojom::ScannerOperationResult::kSuccess);
  ASSERT_TRUE(output->options.has_value());
  auto value = output->options.value().find("option-name");
  ASSERT_TRUE(value != output->options.value().end());
  EXPECT_EQ(value->second->name, "option-name");
}

TEST(DocumentScanAshTypeConvertersTest, GetOptionGroupsResponse_EmptyObject) {
  lorgnette::GetCurrentConfigResponse input;
  auto output = crosapi::mojom::GetOptionGroupsResponse::From(input);
  EXPECT_TRUE(output->scanner_handle.empty());
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_FALSE(output->groups.has_value());
}

TEST(DocumentScanAshTypeConvertersTest, GetOptionGroupsResponse_Success) {
  lorgnette::GetCurrentConfigResponse input;
  input.mutable_scanner()->set_token("scanner-handle");
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerConfig* config = input.mutable_config();
  lorgnette::OptionGroup* group1 = config->add_option_groups();
  group1->set_title("group1");
  group1->add_members("group1-val1");
  group1->add_members("group1-val2");
  lorgnette::OptionGroup* group2 = config->add_option_groups();
  group2->set_title("group2");
  group2->add_members("group2-val1");

  auto output = crosapi::mojom::GetOptionGroupsResponse::From(input);
  EXPECT_EQ(output->scanner_handle, "scanner-handle");
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_TRUE(output->groups.has_value());
  ASSERT_EQ(output->groups.value().size(), 2U);
  const auto& actual1 = output->groups.value()[0];
  EXPECT_EQ(actual1->title, "group1");
  EXPECT_THAT(actual1->members, ElementsAre("group1-val1", "group1-val2"));
  const auto& actual2 = output->groups.value()[1];
  EXPECT_EQ(actual2->title, "group2");
  EXPECT_THAT(actual2->members, ElementsAre("group2-val1"));
}

TEST(DocumentScanAshTypeConvertersTest, CancelScanResponse_EmptyObject) {
  lorgnette::CancelScanResponse input;
  auto output = crosapi::mojom::CancelScanResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kUnknown);
  EXPECT_TRUE(output->job_handle.empty());
}

TEST(DocumentScanAshTypeConvertersTest, CancelScanResponse_Success) {
  lorgnette::CancelScanResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_job_handle()->set_token("job-handle");

  auto output = crosapi::mojom::CancelScanResponse::From(input);
  EXPECT_EQ(output->result, crosapi::mojom::ScannerOperationResult::kSuccess);
  EXPECT_EQ(output->job_handle, "job-handle");
}

}  // namespace
}  // namespace mojo
