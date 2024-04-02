// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

#include "base/containers/contains.h"
#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

namespace document_scan = extensions::api::document_scan;
namespace mojom = crosapi::mojom;

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

TEST(DocumentScanTypeConvertersTest, OperationResult) {
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kUnknown),
            document_scan::OperationResult::kUnknown);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kSuccess),
            document_scan::OperationResult::kSuccess);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kUnsupported),
            document_scan::OperationResult::kUnsupported);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kCancelled),
            document_scan::OperationResult::kCancelled);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceBusy),
            document_scan::OperationResult::kDeviceBusy);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kInvalid),
            document_scan::OperationResult::kInvalid);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kWrongType),
            document_scan::OperationResult::kWrongType);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kEndOfData),
            document_scan::OperationResult::kEof);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAdfJammed),
            document_scan::OperationResult::kAdfJammed);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAdfEmpty),
            document_scan::OperationResult::kAdfEmpty);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kCoverOpen),
            document_scan::OperationResult::kCoverOpen);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kIoError),
            document_scan::OperationResult::kIoError);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kAccessDenied),
            document_scan::OperationResult::kAccessDenied);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kNoMemory),
            document_scan::OperationResult::kNoMemory);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceUnreachable),
            document_scan::OperationResult::kUnreachable);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kDeviceMissing),
            document_scan::OperationResult::kMissing);
  EXPECT_EQ(ConvertTo<document_scan::OperationResult>(
                mojom::ScannerOperationResult::kInternalError),
            document_scan::OperationResult::kInternalError);
}

TEST(DocumentScanTypeConvertersTest, OptionType) {
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kUnknown),
            document_scan::OptionType::kUnknown);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kBool),
            document_scan::OptionType::kBool);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kInt),
            document_scan::OptionType::kInt);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kFixed),
            document_scan::OptionType::kFixed);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kString),
            document_scan::OptionType::kString);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kButton),
            document_scan::OptionType::kButton);
  EXPECT_EQ(ConvertForTesting(mojom::OptionType::kGroup),
            document_scan::OptionType::kGroup);
}

TEST(DocumentScanTypeConvertersTest, OptionUnit) {
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kUnitless),
            document_scan::OptionUnit::kUnitless);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kPixel),
            document_scan::OptionUnit::kPixel);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kBit),
            document_scan::OptionUnit::kBit);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kMm),
            document_scan::OptionUnit::kMm);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kDpi),
            document_scan::OptionUnit::kDpi);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kPercent),
            document_scan::OptionUnit::kPercent);
  EXPECT_EQ(ConvertForTesting(mojom::OptionUnit::kMicrosecond),
            document_scan::OptionUnit::kMicrosecond);
}

TEST(DocumentScanTypeConvertersTest, ConstraintType) {
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kNone),
            document_scan::ConstraintType::kNone);
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kIntRange),
            document_scan::ConstraintType::kIntRange);
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kFixedRange),
            document_scan::ConstraintType::kFixedRange);
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kIntList),
            document_scan::ConstraintType::kIntList);
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kFixedList),
            document_scan::ConstraintType::kFixedList);
  EXPECT_EQ(ConvertForTesting(mojom::OptionConstraintType::kStringList),
            document_scan::ConstraintType::kStringList);
}

TEST(DocumentScanTypeConvertersTest, Configurability) {
  EXPECT_EQ(ConvertForTesting(mojom::OptionConfigurability::kNotConfigurable),
            document_scan::Configurability::kNotConfigurable);
  EXPECT_EQ(
      ConvertForTesting(mojom::OptionConfigurability::kSoftwareConfigurable),
      document_scan::Configurability::kSoftwareConfigurable);
  EXPECT_EQ(
      ConvertForTesting(mojom::OptionConfigurability::kHardwareConfigurable),
      document_scan::Configurability::kHardwareConfigurable);
}

TEST(DocumentScanTypeConvertersTest, Constraint_Empty) {
  auto input = mojom::OptionConstraint::New();
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kNone);
  EXPECT_FALSE(output.list.has_value());
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
}

TEST(DocumentScanTypeConvertersTest, Constraint_IntList) {
  auto input = mojom::OptionConstraint::New();
  input->type = mojom::OptionConstraintType::kIntList;
  input->restriction = mojom::OptionConstraintRestriction::NewValidInt({2, 3});
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kIntList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_integers.has_value());
  EXPECT_THAT(output.list->as_integers.value(), ElementsAre(2, 3));
}

TEST(DocumentScanTypeConvertersTest, Constraint_FixedList) {
  auto input = mojom::OptionConstraint::New();
  input->type = mojom::OptionConstraintType::kFixedList;
  input->restriction =
      mojom::OptionConstraintRestriction::NewValidFixed({4.0, 1.5});
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kFixedList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_numbers.has_value());
  EXPECT_THAT(output.list->as_numbers.value(), ElementsAre(4.0, 1.5));
}

TEST(DocumentScanTypeConvertersTest, Constraint_StringList) {
  auto input = mojom::OptionConstraint::New();
  input->type = mojom::OptionConstraintType::kStringList;
  input->restriction =
      mojom::OptionConstraintRestriction::NewValidString({"a", "b"});
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kStringList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_strings.has_value());
  EXPECT_THAT(output.list->as_strings.value(), ElementsAre("a", "b"));
}

TEST(DocumentScanTypeConvertersTest, Constraint_IntRange) {
  auto input = mojom::OptionConstraint::New();
  input->type = mojom::OptionConstraintType::kIntRange;
  auto restriction = mojom::IntRange::New();
  restriction->min = 1;
  restriction->max = 10;
  restriction->quant = 3;
  input->restriction =
      mojom::OptionConstraintRestriction::NewIntRange(std::move(restriction));
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kIntRange);
  EXPECT_FALSE(output.list.has_value());
  ASSERT_TRUE(output.min.has_value());
  ASSERT_TRUE(output.min->as_integer.has_value());
  EXPECT_EQ(output.min->as_integer.value(), 1);
  ASSERT_TRUE(output.max->as_integer.has_value());
  ASSERT_TRUE(output.max.has_value());
  EXPECT_EQ(output.max->as_integer.value(), 10);
  ASSERT_TRUE(output.quant.has_value());
  ASSERT_TRUE(output.quant->as_integer.has_value());
  EXPECT_EQ(output.quant->as_integer.value(), 3);
}

TEST(DocumentScanTypeConvertersTest, Constraint_FixedRange) {
  auto input = mojom::OptionConstraint::New();
  input->type = mojom::OptionConstraintType::kFixedRange;
  auto restriction = mojom::FixedRange::New();
  restriction->min = 1.5;
  restriction->max = 10.0;
  restriction->quant = 0.5;
  input->restriction =
      mojom::OptionConstraintRestriction::NewFixedRange(std::move(restriction));
  document_scan::OptionConstraint output = ConvertForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kFixedRange);
  EXPECT_FALSE(output.list.has_value());
  ASSERT_TRUE(output.min.has_value());
  ASSERT_TRUE(output.min->as_number.has_value());
  EXPECT_EQ(output.min->as_number.value(), 1.5);
  ASSERT_TRUE(output.max->as_number.has_value());
  ASSERT_TRUE(output.max.has_value());
  EXPECT_EQ(output.max->as_number.value(), 10.0);
  ASSERT_TRUE(output.quant.has_value());
  ASSERT_TRUE(output.quant->as_number.has_value());
  EXPECT_EQ(output.quant->as_number.value(), 0.5);
}

TEST(DocumentScanTypeConvertersTest, Value_BoolValue) {
  auto input = mojom::OptionValue::NewBoolValue(true);
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  ASSERT_TRUE(output.as_boolean.has_value());
  EXPECT_EQ(output.as_boolean.value(), true);
  EXPECT_FALSE(output.as_integer.has_value());
  EXPECT_FALSE(output.as_integers.has_value());
  EXPECT_FALSE(output.as_number.has_value());
  EXPECT_FALSE(output.as_numbers.has_value());
  EXPECT_FALSE(output.as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_IntValue) {
  auto input = mojom::OptionValue::NewIntValue(42);
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  EXPECT_FALSE(output.as_boolean.has_value());
  ASSERT_TRUE(output.as_integer.has_value());
  EXPECT_EQ(output.as_integer.value(), 42);
  EXPECT_FALSE(output.as_integers.has_value());
  EXPECT_FALSE(output.as_number.has_value());
  EXPECT_FALSE(output.as_numbers.has_value());
  EXPECT_FALSE(output.as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_FixedValue) {
  auto input = mojom::OptionValue::NewFixedValue(42.5);
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  EXPECT_FALSE(output.as_boolean.has_value());
  EXPECT_FALSE(output.as_integer.has_value());
  EXPECT_FALSE(output.as_integers.has_value());
  ASSERT_TRUE(output.as_number.has_value());
  EXPECT_EQ(output.as_number.value(), 42.5);
  EXPECT_FALSE(output.as_numbers.has_value());
  EXPECT_FALSE(output.as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_StringValue) {
  auto input = mojom::OptionValue::NewStringValue("string");
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  EXPECT_FALSE(output.as_boolean.has_value());
  EXPECT_FALSE(output.as_integer.has_value());
  EXPECT_FALSE(output.as_integers.has_value());
  EXPECT_FALSE(output.as_number.has_value());
  EXPECT_FALSE(output.as_numbers.has_value());
  ASSERT_TRUE(output.as_string.has_value());
  EXPECT_EQ(output.as_string.value(), "string");
}

TEST(DocumentScanTypeConvertersTest, Value_IntListValue) {
  auto input = mojom::OptionValue::NewIntList({3, 2, 1});
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  EXPECT_FALSE(output.as_boolean.has_value());
  EXPECT_FALSE(output.as_integer.has_value());
  ASSERT_TRUE(output.as_integers.has_value());
  EXPECT_THAT(output.as_integers.value(), ElementsAre(3, 2, 1));
  EXPECT_FALSE(output.as_number.has_value());
  EXPECT_FALSE(output.as_numbers.has_value());
  EXPECT_FALSE(output.as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_FixedListValue) {
  auto input = mojom::OptionValue::NewFixedList({3.5, 2.25, 1.0});
  document_scan::ScannerOption::Value output = ConvertForTesting(input);
  EXPECT_FALSE(output.as_boolean.has_value());
  EXPECT_FALSE(output.as_integer.has_value());
  EXPECT_FALSE(output.as_integers.has_value());
  EXPECT_FALSE(output.as_number.has_value());
  ASSERT_TRUE(output.as_numbers.has_value());
  EXPECT_THAT(output.as_numbers.value(), ElementsAre(3.5, 2.25, 1.0));
  EXPECT_FALSE(output.as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, ScannerOption_Empty) {
  auto input = mojom::ScannerOption::New();
  document_scan::ScannerOption output = ConvertForTesting(input);
  EXPECT_EQ(output.name, "");
  EXPECT_EQ(output.title, "");
  EXPECT_EQ(output.description, "");
  EXPECT_EQ(output.type, document_scan::OptionType::kUnknown);
  EXPECT_EQ(output.unit, document_scan::OptionUnit::kUnitless);
  EXPECT_FALSE(output.value.has_value());
  EXPECT_FALSE(output.constraint.has_value());
  EXPECT_FALSE(output.is_detectable);
  EXPECT_EQ(output.configurability,
            document_scan::Configurability::kNotConfigurable);
  EXPECT_FALSE(output.is_auto_settable);
  EXPECT_FALSE(output.is_emulated);
  EXPECT_FALSE(output.is_active);
  EXPECT_FALSE(output.is_advanced);
  EXPECT_FALSE(output.is_internal);
}

TEST(DocumentScanTypeConvertersTest, ScannerOption_NonEmpty) {
  auto input = mojom::ScannerOption::New();
  input->name = "name";
  input->title = "title";
  input->description = "description";
  input->type = mojom::OptionType::kInt;
  input->unit = mojom::OptionUnit::kDpi;
  input->value = mojom::OptionValue::NewIntValue(42);
  input->constraint = mojom::OptionConstraint::New();
  input->constraint->type = mojom::OptionConstraintType::kIntList;
  input->constraint->restriction =
      mojom::OptionConstraintRestriction::NewValidInt({5});
  input->isDetectable = true;
  input->configurability = mojom::OptionConfigurability::kSoftwareConfigurable;
  input->isAutoSettable = true;
  input->isEmulated = true;
  input->isActive = true;
  input->isAdvanced = true;
  input->isInternal = true;

  document_scan::ScannerOption output = ConvertForTesting(input);
  EXPECT_EQ(output.name, "name");
  EXPECT_EQ(output.title, "title");
  EXPECT_EQ(output.description, "description");
  EXPECT_EQ(output.type, document_scan::OptionType::kInt);
  EXPECT_EQ(output.unit, document_scan::OptionUnit::kDpi);
  ASSERT_TRUE(output.value.has_value());
  ASSERT_TRUE(output.value->as_integer.has_value());
  EXPECT_EQ(output.value->as_integer.value(), 42);
  ASSERT_TRUE(output.constraint.has_value());
  EXPECT_EQ(output.constraint->type, document_scan::ConstraintType::kIntList);
  ASSERT_TRUE(output.constraint->list.has_value());
  ASSERT_TRUE(output.constraint->list->as_integers.has_value());
  EXPECT_THAT(output.constraint->list->as_integers.value(), ElementsAre(5));
  EXPECT_TRUE(output.is_detectable);
  EXPECT_EQ(output.configurability,
            document_scan::Configurability::kSoftwareConfigurable);
  EXPECT_TRUE(output.is_auto_settable);
  EXPECT_TRUE(output.is_emulated);
  EXPECT_TRUE(output.is_active);
  EXPECT_TRUE(output.is_advanced);
  EXPECT_TRUE(output.is_internal);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Empty) {
  document_scan::DeviceFilter input;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_FALSE(output->local);
  EXPECT_FALSE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Local) {
  document_scan::DeviceFilter input;
  input.local = true;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_TRUE(output->local);
  EXPECT_FALSE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, DeviceFilter_Secure) {
  document_scan::DeviceFilter input;
  input.secure = true;
  auto output = mojom::ScannerEnumFilter::From(input);
  EXPECT_FALSE(output->local);
  EXPECT_TRUE(output->secure);
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Empty) {
  auto input = mojom::GetScannerListResponse::New();
  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_EQ(output.scanners.size(), 0U);
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Usb) {
  auto input = mojom::GetScannerListResponse::New();
  input->result = mojom::ScannerOperationResult::kSuccess;
  auto scanner_in = mojom::ScannerInfo::New();
  scanner_in->id = "12345";
  scanner_in->display_name = "12345 (USB)";
  scanner_in->manufacturer = "GoogleTest";
  scanner_in->model = "USB Scanner";
  scanner_in->device_uuid = "56789";
  scanner_in->connection_type = mojom::ScannerInfo_ConnectionType::kUsb;
  scanner_in->secure = true;
  scanner_in->image_formats = {"image/png", "image/jpeg"};
  // scanner_in->protocol_type is unset.
  input->scanners.emplace_back(std::move(scanner_in));

  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_EQ(output.scanners.size(), 1U);
  const document_scan::ScannerInfo& scanner_out = output.scanners[0];
  EXPECT_EQ(scanner_out.scanner_id, "12345");
  EXPECT_EQ(scanner_out.name, "12345 (USB)");
  EXPECT_EQ(scanner_out.manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out.model, "USB Scanner");
  EXPECT_EQ(scanner_out.device_uuid, "56789");
  EXPECT_EQ(scanner_out.connection_type, document_scan::ConnectionType::kUsb);
  EXPECT_EQ(scanner_out.secure, true);
  EXPECT_THAT(scanner_out.image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out.protocol_type, "");
}

TEST(DocumentScanTypeConvertersTest, GetScannerListResponse_Network) {
  auto input = mojom::GetScannerListResponse::New();
  input->result = mojom::ScannerOperationResult::kNoMemory;
  auto scanner_in = mojom::ScannerInfo::New();
  scanner_in->id = "12345";
  scanner_in->display_name = "12345";
  scanner_in->manufacturer = "GoogleTest";
  scanner_in->model = "Network Scanner";
  scanner_in->device_uuid = "56789";
  scanner_in->connection_type = mojom::ScannerInfo_ConnectionType::kNetwork;
  scanner_in->secure = true;
  scanner_in->image_formats = {"image/png", "image/jpeg"};
  scanner_in->protocol_type = "protocol_type";
  input->scanners.emplace_back(std::move(scanner_in));

  auto output = input.To<document_scan::GetScannerListResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kNoMemory);
  ASSERT_EQ(output.scanners.size(), 1U);
  const document_scan::ScannerInfo& scanner_out = output.scanners[0];
  EXPECT_EQ(scanner_out.scanner_id, "12345");
  EXPECT_EQ(scanner_out.name, "12345");
  EXPECT_EQ(scanner_out.manufacturer, "GoogleTest");
  EXPECT_EQ(scanner_out.model, "Network Scanner");
  EXPECT_EQ(scanner_out.device_uuid, "56789");
  EXPECT_EQ(scanner_out.connection_type,
            document_scan::ConnectionType::kNetwork);
  EXPECT_EQ(scanner_out.secure, true);
  EXPECT_THAT(scanner_out.image_formats,
              UnorderedElementsAre("image/png", "image/jpeg"));
  EXPECT_EQ(scanner_out.protocol_type, "protocol_type");
}

TEST(DocumentScanTypeConvertersTest, OpenScannerResponse_Empty) {
  auto input = mojom::OpenScannerResponse::New();
  auto output = input.To<document_scan::OpenScannerResponse>();
  EXPECT_EQ(output.scanner_id, "");
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_FALSE(output.scanner_handle.has_value());
  EXPECT_FALSE(output.options.has_value());
}

TEST(DocumentScanTypeConvertersTest, OpenScannerResponse_NonEmpty) {
  auto input = mojom::OpenScannerResponse::New();
  input->scanner_id = "scanner_id";
  input->result = mojom::ScannerOperationResult::kSuccess;
  input->scanner_handle = "scanner_handle";
  input->options.emplace();
  input->options.value()["name1"] = mojom::ScannerOption::New();
  input->options.value()["name2"] = mojom::ScannerOption::New();

  auto output = input.To<document_scan::OpenScannerResponse>();
  EXPECT_EQ(output.scanner_id, "scanner_id");
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.scanner_handle.has_value());
  EXPECT_EQ(output.scanner_handle.value(), "scanner_handle");
  ASSERT_TRUE(output.options.has_value());
  EXPECT_TRUE(base::Contains(output.options->additional_properties, "name1"));
  EXPECT_TRUE(base::Contains(output.options->additional_properties, "name2"));
}

TEST(DocumentScanTypeConvertersTest, GetOptionGroupsResponse_Empty) {
  auto input = mojom::GetOptionGroupsResponse::New();
  auto output = input.To<document_scan::GetOptionGroupsResponse>();
  EXPECT_EQ(output.scanner_handle, "");
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_FALSE(output.groups.has_value());
}

TEST(DocumentScanTypeConvertersTest, GetOptionGroupsResponse_NonEmpty) {
  auto input = mojom::GetOptionGroupsResponse::New();
  input->scanner_handle = "scanner_handle";
  input->result = mojom::ScannerOperationResult::kSuccess;
  input->groups.emplace();
  auto input_group = mojom::OptionGroup::New();
  input_group->title = "title";
  input_group->members.emplace_back("item1");
  input_group->members.emplace_back("item2");
  input->groups->emplace_back(std::move(input_group));

  auto output = input.To<document_scan::GetOptionGroupsResponse>();
  EXPECT_EQ(output.scanner_handle, "scanner_handle");
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.groups.has_value());
  ASSERT_EQ(output.groups->size(), 1U);
  EXPECT_EQ(output.groups.value()[0].title, "title");
  EXPECT_THAT(output.groups.value()[0].members, ElementsAre("item1", "item2"));
}

TEST(DocumentScanTypeConvertersTest, CloseScannerResponse_Empty) {
  auto input = mojom::CloseScannerResponse::New();
  auto output = input.To<document_scan::CloseScannerResponse>();
  EXPECT_EQ(output.scanner_handle, "");
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
}

TEST(DocumentScanTypeConvertersTest, CloseScannerResponse_NonEmpty) {
  auto input = mojom::CloseScannerResponse::New();
  input->scanner_handle = "scanner_handle";
  input->result = mojom::ScannerOperationResult::kSuccess;

  auto output = input.To<document_scan::CloseScannerResponse>();
  EXPECT_EQ(output.scanner_handle, "scanner_handle");
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Empty) {
  document_scan::OptionSetting input;
  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "");
  EXPECT_EQ(output->type, mojom::OptionType::kUnknown);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_BoolValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kBool;
  input.value.emplace();
  input.value->as_boolean = true;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kBool);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_bool_value());
  EXPECT_EQ(output->value->get_bool_value(), true);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_IntValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kInt;
  input.value.emplace();
  input.value->as_integer = 42;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kInt);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_int_value());
  EXPECT_EQ(output->value->get_int_value(), 42);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_IntList) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kInt;
  input.value.emplace();
  input.value->as_integers = {42, 10};

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kInt);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_int_list());
  EXPECT_THAT(output->value->get_int_list(), ElementsAre(42, 10));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_FixedValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kFixed;
  input.value.emplace();
  input.value->as_number = 42.25;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kFixed);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_fixed_value());
  EXPECT_EQ(output->value->get_fixed_value(), 42.25);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_FixedList) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kFixed;
  input.value.emplace();
  input.value->as_numbers = {42.5, 10.75};

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kFixed);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_fixed_list());
  EXPECT_THAT(output->value->get_fixed_list(), ElementsAre(42.5, 10.75));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_StringValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kString;
  input.value.emplace();
  input.value->as_string = "hello";

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kString);
  ASSERT_FALSE(output->value.is_null());
  ASSERT_TRUE(output->value->is_string_value());
  EXPECT_EQ(output->value->get_string_value(), "hello");
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Group) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kGroup;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kGroup);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Button) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kButton;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kButton);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_None) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kNone;

  auto output = mojom::OptionSetting::From(input);
  EXPECT_EQ(output->name, "name");
  EXPECT_EQ(output->type, mojom::OptionType::kUnknown);
  EXPECT_TRUE(output->value.is_null());
}

TEST(DocumentScanTypeConvertersTest, SetOptionsResponse_Empty) {
  auto input = mojom::SetOptionsResponse::New();
  auto output = input.To<document_scan::SetOptionsResponse>();
  EXPECT_EQ(output.scanner_handle, "");
  EXPECT_TRUE(output.results.empty());
  EXPECT_FALSE(output.options.has_value());
}

TEST(DocumentScanTypeConvertersTest, SetOptionsResponse_NonEmpty) {
  auto input = mojom::SetOptionsResponse::New();
  input->scanner_handle = "scanner-handle";
  input->results.emplace_back(mojom::SetOptionResult::New(
      "name1", mojom::ScannerOperationResult::kWrongType));
  input->results.emplace_back(mojom::SetOptionResult::New(
      "name2", mojom::ScannerOperationResult::kSuccess));
  input->options.emplace();
  input->options->try_emplace(
      "option1", extensions::CreateTestScannerOption("option1", 5));
  input->options->try_emplace(
      "option2", extensions::CreateTestScannerOption("option2", 10));

  auto output = input.To<document_scan::SetOptionsResponse>();
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
  ASSERT_EQ(output.results.size(), 2U);
  EXPECT_EQ(output.results[0].name, "name1");
  EXPECT_EQ(output.results[0].result,
            document_scan::OperationResult::kWrongType);
  EXPECT_EQ(output.results[1].name, "name2");
  EXPECT_EQ(output.results[1].result, document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.options.has_value());
  EXPECT_TRUE(base::Contains(output.options->additional_properties, "option1"));
  EXPECT_TRUE(base::Contains(output.options->additional_properties, "option2"));
}

TEST(DocumentScanTypeConvertersTest, StartScanOptions_Empty) {
  document_scan::StartScanOptions input;
  auto output = mojom::StartScanOptions::From(input);
  EXPECT_TRUE(output->format.empty());
}

TEST(DocumentScanTypeConvertersTest, StartScanOptions_Success) {
  document_scan::StartScanOptions input;
  input.format = "format";
  auto output = mojom::StartScanOptions::From(input);
  EXPECT_EQ(output->format, "format");
  EXPECT_FALSE(output->max_read_size.has_value());
}

TEST(DocumentScanTypeConvertersTest, StartScanOptions_WithMaxReadSize) {
  document_scan::StartScanOptions input;
  input.format = "format";
  input.max_read_size = 100000;
  auto output = mojom::StartScanOptions::From(input);
  EXPECT_EQ(output->format, "format");
  ASSERT_TRUE(output->max_read_size.has_value());
  EXPECT_EQ(output->max_read_size.value(), 100000U);
}

TEST(DocumentScanTypeConvertersTest, StartScanResponse_Empty) {
  auto input = mojom::StartPreparedScanResponse::New();
  auto output = input.To<document_scan::StartScanResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_TRUE(output.scanner_handle.empty());
  EXPECT_FALSE(output.job.has_value());
}

TEST(DocumentScanTypeConvertersTest, StartScanResponse_Success) {
  auto input = mojom::StartPreparedScanResponse::New();
  input->scanner_handle = "scanner-handle";
  input->result = mojom::ScannerOperationResult::kSuccess;
  input->job_handle = "job-handle";

  auto output = input.To<document_scan::StartScanResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
  ASSERT_TRUE(output.job.has_value());
  EXPECT_EQ(output.job.value(), "job-handle");
}

TEST(DocumentScanTypeConvertersTest, CancelScanResponse_Empty) {
  auto input = mojom::CancelScanResponse::New();
  auto output = input.To<document_scan::CancelScanResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_TRUE(output.job.empty());
}

TEST(DocumentScanTypeConvertersTest, CancelScanResponse_Success) {
  auto input = mojom::CancelScanResponse::New();
  input->job_handle = "job-handle";
  input->result = mojom::ScannerOperationResult::kSuccess;

  auto output = input.To<document_scan::CancelScanResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  EXPECT_EQ(output.job, "job-handle");
}

TEST(DocumentScanTypeConvertersTest, ReadScanDataResponse_Empty) {
  auto input = mojom::ReadScanDataResponse::New();
  auto output = input.To<document_scan::ReadScanDataResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_TRUE(output.job.empty());
  EXPECT_FALSE(output.data.has_value());
  EXPECT_FALSE(output.estimated_completion.has_value());
}

TEST(DocumentScanTypeConvertersTest, ReadScanDataResponse_NonEmpty) {
  auto input = mojom::ReadScanDataResponse::New();
  input->job_handle = "job-handle";
  input->result = mojom::ScannerOperationResult::kEndOfData;
  input->data = std::vector<int8_t>('a', 10 * 1024 * 1024);
  input->estimated_completion = 42;

  auto output = input.To<document_scan::ReadScanDataResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kEof);
  EXPECT_EQ(output.job, "job-handle");
  ASSERT_TRUE(output.data.has_value());
  EXPECT_THAT(output.data.value(),
              ElementsAreArray(input->data->data(), input->data->size()));
  ASSERT_TRUE(output.estimated_completion.has_value());
  EXPECT_EQ(output.estimated_completion.value(), 42);
}

TEST(DocumentScanTypeConvertersTest, ReadScanDataResponse_ZeroData) {
  auto input = mojom::ReadScanDataResponse::New();
  input->job_handle = "job-handle";
  input->result = mojom::ScannerOperationResult::kEndOfData;
  input->data = std::vector<int8_t>{};
  input->estimated_completion = 42;

  auto output = input.To<document_scan::ReadScanDataResponse>();
  EXPECT_EQ(output.result, document_scan::OperationResult::kEof);
  EXPECT_EQ(output.job, "job-handle");
  ASSERT_TRUE(output.data.has_value());
  EXPECT_EQ(output.data.value().size(), 0U);
  ASSERT_TRUE(output.estimated_completion.has_value());
  EXPECT_EQ(output.estimated_completion.value(), 42);
}

}  // namespace
}  // namespace mojo
