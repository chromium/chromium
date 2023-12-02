// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

#include "base/containers/contains.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

namespace document_scan = extensions::api::document_scan;
namespace mojom = crosapi::mojom;

using ::testing::ElementsAre;
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

}  // namespace
}  // namespace mojo
