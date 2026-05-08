// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

#include <algorithm>
#include <optional>

#include "chrome/browser/extensions/api/document_scan/document_scan_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace document_scan = extensions::api::document_scan;

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::UnorderedElementsAre;

std::optional<document_scan::OperationResult> GetResultByName(
    const std::vector<document_scan::SetOptionResult>& results,
    const std::string& name) {
  auto it = std::ranges::find_if(results,
                                 [&](const auto& r) { return r.name == name; });
  return it != results.end() ? std::make_optional(it->result) : std::nullopt;
}

TEST(DocumentScanTypeConvertersTest, OptionType) {
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionTypeForTesting(
                lorgnette::TYPE_UNKNOWN),
            document_scan::OptionType::kUnknown);
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionTypeForTesting(lorgnette::TYPE_BOOL),
      document_scan::OptionType::kBool);
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionTypeForTesting(lorgnette::TYPE_INT),
      document_scan::OptionType::kInt);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionTypeForTesting(
                lorgnette::TYPE_FIXED),
            document_scan::OptionType::kFixed);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionTypeForTesting(
                lorgnette::TYPE_STRING),
            document_scan::OptionType::kString);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionTypeForTesting(
                lorgnette::TYPE_BUTTON),
            document_scan::OptionType::kButton);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionTypeForTesting(
                lorgnette::TYPE_GROUP),
            document_scan::OptionType::kGroup);
}

TEST(DocumentScanTypeConvertersTest, OptionUnit) {
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionUnitForTesting(lorgnette::UNIT_NONE),
      document_scan::OptionUnit::kUnitless);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionUnitForTesting(
                lorgnette::UNIT_PIXEL),
            document_scan::OptionUnit::kPixel);
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionUnitForTesting(lorgnette::UNIT_BIT),
      document_scan::OptionUnit::kBit);
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionUnitForTesting(lorgnette::UNIT_MM),
      document_scan::OptionUnit::kMm);
  EXPECT_EQ(
      document_scan::ConvertLorgnetteOptionUnitForTesting(lorgnette::UNIT_DPI),
      document_scan::OptionUnit::kDpi);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionUnitForTesting(
                lorgnette::UNIT_PERCENT),
            document_scan::OptionUnit::kPercent);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionUnitForTesting(
                lorgnette::UNIT_MICROSECOND),
            document_scan::OptionUnit::kMicrosecond);
}

TEST(DocumentScanTypeConvertersTest, ConstraintType) {
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_NONE),
            document_scan::ConstraintType::kNone);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE),
            document_scan::ConstraintType::kIntRange);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE),
            document_scan::ConstraintType::kFixedRange);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_INT_LIST),
            document_scan::ConstraintType::kIntList);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST),
            document_scan::ConstraintType::kFixedList);
  EXPECT_EQ(document_scan::ConvertLorgnetteOptionConstraintTypeForTesting(
                lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST),
            document_scan::ConstraintType::kStringList);
}

TEST(DocumentScanTypeConvertersTest, Configurability) {
  lorgnette::ScannerOption input;

  input.set_sw_settable(false);
  input.set_hw_settable(false);
  EXPECT_EQ(document_scan::ConvertLorgnetteScannerOptionForTesting(input)
                .configurability,
            document_scan::Configurability::kNotConfigurable);

  input.set_sw_settable(true);
  input.set_hw_settable(false);
  EXPECT_EQ(document_scan::ConvertLorgnetteScannerOptionForTesting(input)
                .configurability,
            document_scan::Configurability::kSoftwareConfigurable);

  input.set_sw_settable(false);
  input.set_hw_settable(true);
  EXPECT_EQ(document_scan::ConvertLorgnetteScannerOptionForTesting(input)
                .configurability,
            document_scan::Configurability::kHardwareConfigurable);
}

TEST(DocumentScanTypeConvertersTest, Constraint_Empty) {
  lorgnette::OptionConstraint input;
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kNone);
  EXPECT_FALSE(output.list.has_value());
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
}

TEST(DocumentScanTypeConvertersTest, Constraint_IntList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_LIST);
  input.add_valid_int(2);
  input.add_valid_int(3);
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kIntList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_integers.has_value());
  EXPECT_THAT(output.list->as_integers.value(), ElementsAre(2, 3));
}

TEST(DocumentScanTypeConvertersTest, Constraint_FixedList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST);
  input.add_valid_fixed(4.0);
  input.add_valid_fixed(1.5);
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kFixedList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_numbers.has_value());
  EXPECT_THAT(output.list->as_numbers.value(), ElementsAre(4.0, 1.5));
}

TEST(DocumentScanTypeConvertersTest, Constraint_StringList) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST);
  input.add_valid_string("a");
  input.add_valid_string("b");
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
  EXPECT_EQ(output.type, document_scan::ConstraintType::kStringList);
  EXPECT_FALSE(output.min.has_value());
  EXPECT_FALSE(output.max.has_value());
  EXPECT_FALSE(output.quant.has_value());
  ASSERT_TRUE(output.list.has_value());
  ASSERT_TRUE(output.list->as_strings.has_value());
  EXPECT_THAT(output.list->as_strings.value(), ElementsAre("a", "b"));
}

TEST(DocumentScanTypeConvertersTest, Constraint_IntRange) {
  lorgnette::OptionConstraint input;
  input.set_constraint_type(lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE);
  auto* restriction = input.mutable_int_range();
  restriction->set_min(1);
  restriction->set_max(10);
  restriction->set_quant(3);
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
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
  lorgnette::OptionConstraint input;
  input.set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE);
  auto* restriction = input.mutable_fixed_range();
  restriction->set_min(1.5);
  restriction->set_max(10.0);
  restriction->set_quant(0.5);
  document_scan::OptionConstraint output =
      document_scan::ConvertLorgnetteOptionConstraintForTesting(input);
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
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_BOOL);
  input.set_bool_value(true);
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  ASSERT_TRUE(output->as_boolean.has_value());
  EXPECT_EQ(output->as_boolean.value(), true);
  EXPECT_FALSE(output->as_integer.has_value());
  EXPECT_FALSE(output->as_integers.has_value());
  EXPECT_FALSE(output->as_number.has_value());
  EXPECT_FALSE(output->as_numbers.has_value());
  EXPECT_FALSE(output->as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_IntValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_INT);
  input.mutable_int_value()->add_value(42);
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_FALSE(output->as_boolean.has_value());
  ASSERT_TRUE(output->as_integer.has_value());
  EXPECT_EQ(output->as_integer.value(), 42);
  EXPECT_FALSE(output->as_integers.has_value());
  EXPECT_FALSE(output->as_number.has_value());
  EXPECT_FALSE(output->as_numbers.has_value());
  EXPECT_FALSE(output->as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_FixedValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_FIXED);
  input.mutable_fixed_value()->add_value(42.5);
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_FALSE(output->as_boolean.has_value());
  EXPECT_FALSE(output->as_integer.has_value());
  EXPECT_FALSE(output->as_integers.has_value());
  ASSERT_TRUE(output->as_number.has_value());
  EXPECT_EQ(output->as_number.value(), 42.5);
  EXPECT_FALSE(output->as_numbers.has_value());
  EXPECT_FALSE(output->as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_StringValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_STRING);
  input.set_string_value("string");
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_FALSE(output->as_boolean.has_value());
  EXPECT_FALSE(output->as_integer.has_value());
  EXPECT_FALSE(output->as_integers.has_value());
  EXPECT_FALSE(output->as_number.has_value());
  EXPECT_FALSE(output->as_numbers.has_value());
  ASSERT_TRUE(output->as_string.has_value());
  EXPECT_EQ(output->as_string.value(), "string");
}

TEST(DocumentScanTypeConvertersTest, Value_IntListValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_INT);
  input.mutable_int_value()->add_value(3);
  input.mutable_int_value()->add_value(2);
  input.mutable_int_value()->add_value(1);
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_FALSE(output->as_boolean.has_value());
  EXPECT_FALSE(output->as_integer.has_value());
  ASSERT_TRUE(output->as_integers.has_value());
  EXPECT_THAT(output->as_integers.value(), ElementsAre(3, 2, 1));
  EXPECT_FALSE(output->as_number.has_value());
  EXPECT_FALSE(output->as_numbers.has_value());
  EXPECT_FALSE(output->as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, Value_FixedListValue) {
  lorgnette::ScannerOption input;
  input.set_option_type(lorgnette::TYPE_FIXED);
  input.mutable_fixed_value()->add_value(3.5);
  input.mutable_fixed_value()->add_value(2.25);
  input.mutable_fixed_value()->add_value(1.0);
  std::optional<document_scan::ScannerOption::Value> output =
      document_scan::GetLorgnetteOptionValueForTesting(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_FALSE(output->as_boolean.has_value());
  EXPECT_FALSE(output->as_integer.has_value());
  EXPECT_FALSE(output->as_integers.has_value());
  EXPECT_FALSE(output->as_number.has_value());
  ASSERT_TRUE(output->as_numbers.has_value());
  EXPECT_THAT(output->as_numbers.value(), ElementsAre(3.5, 2.25, 1.0));
  EXPECT_FALSE(output->as_string.has_value());
}

TEST(DocumentScanTypeConvertersTest, ScannerOption_Empty) {
  lorgnette::ScannerOption input;
  document_scan::ScannerOption output =
      document_scan::ConvertLorgnetteScannerOptionForTesting(input);
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
  lorgnette::ScannerOption input;
  input.set_name("name");
  input.set_title("title");
  input.set_description("description");
  input.set_option_type(lorgnette::TYPE_INT);
  input.set_unit(lorgnette::UNIT_DPI);
  input.mutable_int_value()->add_value(42);
  auto* constraint = input.mutable_constraint();
  constraint->set_constraint_type(
      lorgnette::OptionConstraint::CONSTRAINT_INT_LIST);
  constraint->add_valid_int(5);
  input.set_detectable(true);
  input.set_sw_settable(true);
  input.set_auto_settable(true);
  input.set_emulated(true);
  input.set_active(true);
  input.set_advanced(true);

  document_scan::ScannerOption output =
      document_scan::ConvertLorgnetteScannerOptionForTesting(input);
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
  EXPECT_FALSE(output.is_internal);
}



TEST(DocumentScanTypeConvertersTest, OpenScannerResponse_Empty) {
  lorgnette::OpenScannerResponse input;
  auto output = document_scan::ConvertLorgnetteOpenScannerResponse(input);
  EXPECT_EQ(output.scanner_id, "");
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_FALSE(output.scanner_handle.has_value());
  EXPECT_FALSE(output.options.has_value());
}

TEST(DocumentScanTypeConvertersTest, OpenScannerResponse_NonEmpty) {
  lorgnette::OpenScannerResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_scanner_id()->set_connection_string("scanner_id");
  lorgnette::ScannerConfig* config = input.mutable_config();
  config->mutable_scanner()->set_token("scanner_handle");

  // Int option
  (*config->mutable_options())["option_int"] =
      extensions::CreateTestScannerOption("option_int", 42);

  // Bool option
  lorgnette::ScannerOption bool_option;
  bool_option.set_name("option_bool");
  bool_option.set_option_type(lorgnette::TYPE_BOOL);
  bool_option.set_bool_value(true);
  (*config->mutable_options())["option_bool"] = bool_option;

  // String option
  lorgnette::ScannerOption string_option;
  string_option.set_name("option_string");
  string_option.set_option_type(lorgnette::TYPE_STRING);
  string_option.set_string_value("hello");
  (*config->mutable_options())["option_string"] = string_option;

  // Fixed option
  lorgnette::ScannerOption fixed_option;
  fixed_option.set_name("option_fixed");
  fixed_option.set_option_type(lorgnette::TYPE_FIXED);
  fixed_option.mutable_fixed_value()->add_value(42.5);
  (*config->mutable_options())["option_fixed"] = fixed_option;

  // Int list option
  lorgnette::ScannerOption int_list_option;
  int_list_option.set_name("option_int_list");
  int_list_option.set_option_type(lorgnette::TYPE_INT);
  int_list_option.mutable_int_value()->add_value(1);
  int_list_option.mutable_int_value()->add_value(2);
  (*config->mutable_options())["option_int_list"] = int_list_option;

  // Fixed list option
  lorgnette::ScannerOption fixed_list_option;
  fixed_list_option.set_name("option_fixed_list");
  fixed_list_option.set_option_type(lorgnette::TYPE_FIXED);
  fixed_list_option.mutable_fixed_value()->add_value(1.5);
  fixed_list_option.mutable_fixed_value()->add_value(2.5);
  (*config->mutable_options())["option_fixed_list"] = fixed_list_option;

  auto output = document_scan::ConvertLorgnetteOpenScannerResponse(input);
  EXPECT_EQ(output.scanner_id, "scanner_id");
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.scanner_handle.has_value());
  EXPECT_EQ(output.scanner_handle.value(), "scanner_handle");
  ASSERT_TRUE(output.options.has_value());
  EXPECT_TRUE(output.options->additional_properties.contains("option_int"));
  EXPECT_TRUE(output.options->additional_properties.contains("option_bool"));
  EXPECT_TRUE(output.options->additional_properties.contains("option_string"));
  EXPECT_TRUE(output.options->additional_properties.contains("option_fixed"));
  EXPECT_TRUE(
      output.options->additional_properties.contains("option_int_list"));
  EXPECT_TRUE(
      output.options->additional_properties.contains("option_fixed_list"));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Empty) {
  document_scan::OptionSetting input;
  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_UNKNOWN);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_BoolValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kBool;
  input.value.emplace();
  input.value->as_boolean = true;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_BOOL);
  EXPECT_TRUE(output->has_bool_value());
  EXPECT_TRUE(output->bool_value());
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_IntValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kInt;
  input.value.emplace();
  input.value->as_integer = 42;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  ASSERT_TRUE(output->has_int_value());
  EXPECT_THAT(output->int_value().value(), ElementsAre(42));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_IntListValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kInt;
  input.value.emplace();
  input.value->as_integers = {42, 10};

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  ASSERT_TRUE(output->has_int_value());
  EXPECT_THAT(output->int_value().value(), ElementsAre(42, 10));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_FixedValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kFixed;
  input.value.emplace();
  input.value->as_number = 42.25;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_FIXED);
  ASSERT_TRUE(output->has_fixed_value());
  EXPECT_THAT(output->fixed_value().value(), ElementsAre(42.25));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_FixedListValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kFixed;
  input.value.emplace();
  input.value->as_numbers = {42.5, 10.75};

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_FIXED);
  ASSERT_TRUE(output->has_fixed_value());
  EXPECT_THAT(output->fixed_value().value(), ElementsAre(42.5, 10.75));
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_StringValue) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kString;
  input.value.emplace();
  input.value->as_string = "hello";

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_STRING);
  EXPECT_TRUE(output->has_string_value());
  EXPECT_EQ(output->string_value(), "hello");
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Group) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kGroup;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_GROUP);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_Button) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kButton;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_BUTTON);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_None) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kNone;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "name");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_UNKNOWN);
}

TEST(DocumentScanTypeConvertersTest, OptionSetting_TypeMismatch) {
  document_scan::OptionSetting input;
  input.name = "name";
  input.type = document_scan::OptionType::kBool;
  input.value.emplace();
  input.value->as_integer = 42;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  EXPECT_FALSE(output.has_value());
}

TEST(DocumentScanTypeConvertersTest, SetOptionsResponse_Empty) {
  lorgnette::SetOptionsResponse input;
  auto output = document_scan::TransformLorgnetteSetOptionsResponse(input, {});
  EXPECT_EQ(output.scanner_handle, "");
  EXPECT_TRUE(output.results.empty());
  EXPECT_FALSE(output.options.has_value());
}

TEST(DocumentScanTypeConvertersTest, SetOptionsResponse_NonEmpty) {
  lorgnette::SetOptionsResponse input;
  input.mutable_scanner()->set_token("scanner-handle");
  (*input.mutable_results())["name1"] = lorgnette::OPERATION_RESULT_WRONG_TYPE;
  (*input.mutable_results())["name2"] = lorgnette::OPERATION_RESULT_SUCCESS;
  lorgnette::ScannerConfig* config = input.mutable_config();
  (*config->mutable_options())["option1"] =
      extensions::CreateTestScannerOption("option1", 5);
  (*config->mutable_options())["option2"] =
      extensions::CreateTestScannerOption("option2", 10);

  auto output = document_scan::TransformLorgnetteSetOptionsResponse(input, {});
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
  EXPECT_EQ(output.results.size(), 2U);
  EXPECT_EQ(GetResultByName(output.results, "name1"),
            document_scan::OperationResult::kWrongType);
  EXPECT_EQ(GetResultByName(output.results, "name2"),
            document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.options.has_value());
  EXPECT_TRUE(output.options->additional_properties.contains("option1"));
  EXPECT_TRUE(output.options->additional_properties.contains("option2"));
}

TEST(DocumentScanTypeConvertersTest, StartScanResponse_Empty) {
  lorgnette::StartPreparedScanResponse input;
  auto output = document_scan::ConvertLorgnetteStartPreparedScanResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_TRUE(output.scanner_handle.empty());
  EXPECT_FALSE(output.job.has_value());
}

TEST(DocumentScanTypeConvertersTest, StartScanResponse_Success) {
  lorgnette::StartPreparedScanResponse input;
  input.mutable_scanner()->set_token("scanner-handle");
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_job_handle()->set_token("job-handle");

  auto output = document_scan::ConvertLorgnetteStartPreparedScanResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
  ASSERT_TRUE(output.job.has_value());
  EXPECT_EQ(output.job.value(), "job-handle");
}

TEST(DocumentScanTypeConvertersTest,
     ConvertLorgnetteReadScanDataResponse_Empty) {
  lorgnette::ReadScanDataResponse input;

  auto output = document_scan::ConvertLorgnetteReadScanDataResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kUnknown);
  EXPECT_TRUE(output.job.empty());
  EXPECT_FALSE(output.data.has_value());
  EXPECT_FALSE(output.estimated_completion.has_value());
}

TEST(DocumentScanTypeConvertersTest,
     ConvertLorgnetteReadScanDataResponse_NonEmpty) {
  lorgnette::ReadScanDataResponse input;
  input.mutable_job_handle()->set_token("job-handle");
  input.set_result(lorgnette::OPERATION_RESULT_EOF);
  input.set_data(std::string(10 * 1024 * 1024, 'a'));
  input.set_estimated_completion(42);

  auto output = document_scan::ConvertLorgnetteReadScanDataResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kEof);
  EXPECT_EQ(output.job, "job-handle");
  ASSERT_TRUE(output.data.has_value());
  EXPECT_THAT(output.data.value(),
              ElementsAreArray(input.data().data(), input.data().size()));
  ASSERT_TRUE(output.estimated_completion.has_value());
  EXPECT_EQ(output.estimated_completion.value(), 42);
}

TEST(DocumentScanTypeConvertersTest,
     ConvertLorgnetteReadScanDataResponse_ZeroData) {
  lorgnette::ReadScanDataResponse input;
  input.mutable_job_handle()->set_token("job-handle");
  input.set_result(lorgnette::OPERATION_RESULT_EOF);
  input.set_data("");
  input.set_estimated_completion(42);

  auto output = document_scan::ConvertLorgnetteReadScanDataResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kEof);
  EXPECT_EQ(output.job, "job-handle");
  ASSERT_TRUE(output.data.has_value());
  EXPECT_EQ(output.data.value().size(), 0U);
  ASSERT_TRUE(output.estimated_completion.has_value());
  EXPECT_EQ(output.estimated_completion.value(), 42);
}

TEST(DocumentScanTypeConvertersTest, ConvertLorgnetteOperationResult) {
  EXPECT_EQ(document_scan::ConvertLorgnetteOperationResult(
                lorgnette::OPERATION_RESULT_SUCCESS),
            document_scan::OperationResult::kSuccess);
  EXPECT_EQ(document_scan::ConvertLorgnetteOperationResult(
                lorgnette::OPERATION_RESULT_INTERNAL_ERROR),
            document_scan::OperationResult::kInternalError);
}

TEST(DocumentScanTypeConvertersTest, ConvertLorgnetteCancelScanResponse) {
  lorgnette::CancelScanResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_job_handle()->set_token("job-handle");

  auto output = document_scan::ConvertLorgnetteCancelScanResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  EXPECT_EQ(output.job, "job-handle");
}

TEST(DocumentScanTypeConvertersTest, ConvertLorgnetteCloseScannerResponse) {
  lorgnette::CloseScannerResponse input;
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  input.mutable_scanner()->set_token("scanner-handle");

  auto output = document_scan::ConvertLorgnetteCloseScannerResponse(input);
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
}

TEST(DocumentScanTypeConvertersTest, ConvertLorgnetteGetCurrentConfigResponse) {
  lorgnette::GetCurrentConfigResponse input;
  input.mutable_scanner()->set_token("scanner-handle");
  input.set_result(lorgnette::OPERATION_RESULT_SUCCESS);
  lorgnette::ScannerConfig* config = input.mutable_config();
  lorgnette::OptionGroup* group = config->add_option_groups();
  group->set_title("group-title");
  group->add_members("group-member");

  auto output = document_scan::ConvertLorgnetteGetCurrentConfigResponse(input);
  EXPECT_EQ(output.scanner_handle, "scanner-handle");
  EXPECT_EQ(output.result, document_scan::OperationResult::kSuccess);
  ASSERT_TRUE(output.groups.has_value());
  ASSERT_EQ(output.groups->size(), 1U);
  EXPECT_EQ(output.groups.value()[0].title, "group-title");
  EXPECT_THAT(output.groups.value()[0].members, ElementsAre("group-member"));
}

TEST(DocumentScanTypeConvertersTest,
     TransformOptionSettingToLorgnetteScannerOption_AutoSet) {
  document_scan::OptionSetting input;
  input.name = "option1";
  input.type = document_scan::OptionType::kInt;

  auto output =
      document_scan::TransformOptionSettingToLorgnetteScannerOption(input);
  ASSERT_TRUE(output.has_value());
  EXPECT_EQ(output->name(), "option1");
  EXPECT_EQ(output->option_type(), lorgnette::TYPE_INT);
  EXPECT_FALSE(output->has_int_value());
}

}  // namespace
