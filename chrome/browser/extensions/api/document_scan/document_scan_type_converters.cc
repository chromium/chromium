// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

#include <algorithm>

#include "base/check_is_test.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/ash/components/dbus/lorgnette/lorgnette_service.pb.h"

namespace extensions::api::document_scan {

namespace {

OptionType ConvertLorgnetteOptionType(const lorgnette::OptionType& input) {
  switch (input) {
    case lorgnette::TYPE_UNKNOWN:
      return OptionType::kUnknown;
    case lorgnette::TYPE_BOOL:
      return OptionType::kBool;
    case lorgnette::TYPE_INT:
      return OptionType::kInt;
    case lorgnette::TYPE_FIXED:
      return OptionType::kFixed;
    case lorgnette::TYPE_STRING:
      return OptionType::kString;
    case lorgnette::TYPE_BUTTON:
      return OptionType::kButton;
    case lorgnette::TYPE_GROUP:
      return OptionType::kGroup;
    case lorgnette::OptionType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::OptionType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

OptionUnit ConvertLorgnetteOptionUnit(const lorgnette::OptionUnit& input) {
  switch (input) {
    case lorgnette::UNIT_NONE:
      return OptionUnit::kUnitless;
    case lorgnette::UNIT_PIXEL:
      return OptionUnit::kPixel;
    case lorgnette::UNIT_BIT:
      return OptionUnit::kBit;
    case lorgnette::UNIT_MM:
      return OptionUnit::kMm;
    case lorgnette::UNIT_DPI:
      return OptionUnit::kDpi;
    case lorgnette::UNIT_PERCENT:
      return OptionUnit::kPercent;
    case lorgnette::UNIT_MICROSECOND:
      return OptionUnit::kMicrosecond;
    case lorgnette::OptionUnit_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::OptionUnit_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

ConstraintType ConvertLorgnetteOptionConstraintType(
    const lorgnette::OptionConstraint_ConstraintType& input) {
  switch (input) {
    case lorgnette::OptionConstraint::CONSTRAINT_NONE:
      return ConstraintType::kNone;
    case lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE:
      return ConstraintType::kIntRange;
    case lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE:
      return ConstraintType::kFixedRange;
    case lorgnette::OptionConstraint::CONSTRAINT_INT_LIST:
      return ConstraintType::kIntList;
    case lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST:
      return ConstraintType::kFixedList;
    case lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST:
      return ConstraintType::kStringList;
    case lorgnette::
        OptionConstraint_ConstraintType_OptionConstraint_ConstraintType_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::
        OptionConstraint_ConstraintType_OptionConstraint_ConstraintType_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

OptionConstraint ConvertLorgnetteOptionConstraint(
    const lorgnette::OptionConstraint& input) {
  document_scan::OptionConstraint output;
  output.type = ConvertLorgnetteOptionConstraintType(input.constraint_type());
  switch (output.type) {
    case ConstraintType::kNone:
      break;
    case ConstraintType::kIntRange: {
      if (!input.has_int_range()) {
        LOG(WARNING) << "OptionConstraint has type INT_RANGE but does not "
                        "contain a valid int_range";
      }
      auto& input_range = input.int_range();
      output.min.emplace();
      output.min->as_integer = input_range.min();
      output.max.emplace();
      output.max->as_integer = input_range.max();
      output.quant.emplace();
      output.quant->as_integer = input_range.quant();
      break;
    }
    case ConstraintType::kFixedRange: {
      if (!input.has_fixed_range()) {
        LOG(WARNING) << "OptionConstraint has type FIXED_RANGE but does not "
                        "contain a valid fixed_range";
      }
      auto& input_range = input.fixed_range();
      output.min.emplace();
      output.min->as_number = input_range.min();
      output.max.emplace();
      output.max->as_number = input_range.max();
      output.quant.emplace();
      output.quant->as_number = input_range.quant();
      break;
    }
    case ConstraintType::kIntList:
      if (input.valid_int().empty()) {
        LOG(WARNING) << "OptionConstraint has type INT_LIST but does not "
                        "contain a valid valid_int";
      }
      output.list.emplace();
      output.list->as_integers.emplace(input.valid_int().begin(),
                                       input.valid_int().end());
      break;
    case ConstraintType::kFixedList:
      if (input.valid_fixed().empty()) {
        LOG(WARNING) << "OptionConstraint has type FIXED_LIST but does not "
                        "contain a valid valid_fixed";
      }
      output.list.emplace();
      output.list->as_numbers.emplace(input.valid_fixed().begin(),
                                      input.valid_fixed().end());
      break;
    case ConstraintType::kStringList:
      if (input.valid_string().empty()) {
        LOG(WARNING) << "OptionConstraint has type STRING_LIST but does not "
                        "contain a valid valid_string";
      }
      output.list.emplace();
      output.list->as_strings.emplace(input.valid_string().begin(),
                                      input.valid_string().end());
      break;
  }
  return output;
}

std::optional<ScannerOption::Value> GetLorgnetteOptionValue(
    const lorgnette::ScannerOption& option) {
  std::optional<ScannerOption::Value> result;
  switch (ConvertLorgnetteOptionType(option.option_type())) {
    case OptionType::kNone:
    case OptionType::kUnknown:
    case OptionType::kButton:
    case OptionType::kGroup:
      break;
    case OptionType::kBool:
      if (option.has_bool_value()) {
        result.emplace();
        result->as_boolean.emplace(option.bool_value());
      }
      break;
    case OptionType::kInt:
      if (option.has_int_value()) {
        result.emplace();
        if (option.int_value().value_size() == 1) {
          result->as_integer.emplace(option.int_value().value(0));
        } else {
          result->as_integers.emplace(option.int_value().value().begin(),
                                      option.int_value().value().end());
        }
      }
      break;
    case OptionType::kFixed:
      if (option.has_fixed_value()) {
        result.emplace();
        if (option.fixed_value().value_size() == 1) {
          result->as_number.emplace(option.fixed_value().value(0));
        } else {
          result->as_numbers.emplace(option.fixed_value().value().begin(),
                                     option.fixed_value().value().end());
        }
      }
      break;
    case OptionType::kString:
      if (option.has_string_value()) {
        result.emplace();
        result->as_string.emplace(option.string_value());
      }
      break;
  }
  return result;
}

ScannerOption ConvertLorgnetteScannerOption(
    const lorgnette::ScannerOption& input) {
  ScannerOption output;
  output.name = input.name();
  output.title = input.title();
  output.description = input.description();
  output.type = ConvertLorgnetteOptionType(input.option_type());
  output.unit = ConvertLorgnetteOptionUnit(input.unit());
  output.value = GetLorgnetteOptionValue(input);
  output.constraint =
      input.has_constraint()
          ? std::optional(ConvertLorgnetteOptionConstraint(input.constraint()))
          : std::nullopt;
  output.is_detectable = input.detectable();
  output.configurability =
      input.sw_settable()   ? Configurability::kSoftwareConfigurable
      : input.hw_settable() ? Configurability::kHardwareConfigurable
                            : Configurability::kNotConfigurable;
  output.is_auto_settable = input.auto_settable();
  output.is_emulated = input.emulated();
  output.is_active = input.active();
  output.is_advanced = input.advanced();
  output.is_internal = false;
  return output;
}

}  // namespace

OperationResult ConvertLorgnetteOperationResult(
    lorgnette::OperationResult input) {
  switch (input) {
    case lorgnette::OPERATION_RESULT_UNKNOWN:
      return OperationResult::kUnknown;
    case lorgnette::OPERATION_RESULT_SUCCESS:
      return OperationResult::kSuccess;
    case lorgnette::OPERATION_RESULT_UNSUPPORTED:
      return OperationResult::kUnsupported;
    case lorgnette::OPERATION_RESULT_CANCELLED:
      return OperationResult::kCancelled;
    case lorgnette::OPERATION_RESULT_DEVICE_BUSY:
      return OperationResult::kDeviceBusy;
    case lorgnette::OPERATION_RESULT_INVALID:
      return OperationResult::kInvalid;
    case lorgnette::OPERATION_RESULT_WRONG_TYPE:
      return OperationResult::kWrongType;
    case lorgnette::OPERATION_RESULT_EOF:
      return OperationResult::kEof;
    case lorgnette::OPERATION_RESULT_ADF_JAMMED:
      return OperationResult::kAdfJammed;
    case lorgnette::OPERATION_RESULT_ADF_EMPTY:
      return OperationResult::kAdfEmpty;
    case lorgnette::OPERATION_RESULT_COVER_OPEN:
      return OperationResult::kCoverOpen;
    case lorgnette::OPERATION_RESULT_IO_ERROR:
      return OperationResult::kIoError;
    case lorgnette::OPERATION_RESULT_ACCESS_DENIED:
      return OperationResult::kAccessDenied;
    case lorgnette::OPERATION_RESULT_NO_MEMORY:
      return OperationResult::kNoMemory;
    case lorgnette::OPERATION_RESULT_UNREACHABLE:
      return OperationResult::kUnreachable;
    case lorgnette::OPERATION_RESULT_MISSING:
      return OperationResult::kMissing;
    case lorgnette::OPERATION_RESULT_INTERNAL_ERROR:
      return OperationResult::kInternalError;
    case lorgnette::OperationResult_INT_MIN_SENTINEL_DO_NOT_USE_:
    case lorgnette::OperationResult_INT_MAX_SENTINEL_DO_NOT_USE_:
      break;
  }
  NOTREACHED();
}

CancelScanResponse ConvertLorgnetteCancelScanResponse(
    const lorgnette::CancelScanResponse& input) {
  CancelScanResponse output;
  output.job = input.job_handle().token();
  output.result = ConvertLorgnetteOperationResult(input.result());
  return output;
}

CloseScannerResponse ConvertLorgnetteCloseScannerResponse(
    const lorgnette::CloseScannerResponse& input) {
  CloseScannerResponse output;
  output.scanner_handle = input.scanner().token();
  output.result = ConvertLorgnetteOperationResult(input.result());
  return output;
}

GetOptionGroupsResponse ConvertLorgnetteGetCurrentConfigResponse(
    const lorgnette::GetCurrentConfigResponse& input) {
  GetOptionGroupsResponse output;
  output.scanner_handle = input.scanner().token();
  output.result = ConvertLorgnetteOperationResult(input.result());
  if (output.result == OperationResult::kSuccess) {
    output.groups.emplace();
    output.groups->reserve(input.config().option_groups_size());
    for (const lorgnette::OptionGroup& group_in :
         input.config().option_groups()) {
      OptionGroup& group_out = output.groups->emplace_back();
      group_out.title = group_in.title();
      group_out.members = std::vector<std::string>(group_in.members().begin(),
                                                   group_in.members().end());
    }
  }
  return output;
}

StartScanResponse ConvertLorgnetteStartPreparedScanResponse(
    const lorgnette::StartPreparedScanResponse& input) {
  StartScanResponse output;
  output.scanner_handle = input.scanner().token();
  output.result = ConvertLorgnetteOperationResult(input.result());
  if (input.has_job_handle()) {
    output.job = input.job_handle().token();
  }
  return output;
}

ReadScanDataResponse ConvertLorgnetteReadScanDataResponse(
    const lorgnette::ReadScanDataResponse& input) {
  ReadScanDataResponse output;
  output.job = input.job_handle().token();
  output.result = ConvertLorgnetteOperationResult(input.result());
  if (input.has_data()) {
    output.data.emplace(input.data().begin(), input.data().end());
  }
  if (input.has_estimated_completion()) {
    output.estimated_completion = input.estimated_completion();
  }
  return output;
}

SetOptionsResponse TransformLorgnetteSetOptionsResponse(
    const lorgnette::SetOptionsResponse& input,
    const std::vector<std::string>& invalid_option_names) {
  SetOptionsResponse output;
  output.scanner_handle = input.scanner().token();
  for (const auto& [name, result] : input.results()) {
    SetOptionResult& set_option_result = output.results.emplace_back();
    set_option_result.name = name;
    set_option_result.result = ConvertLorgnetteOperationResult(result);
  }
  if (input.has_config()) {
    output.options.emplace();
    for (const auto& [name, option] : input.config().options()) {
      output.options->additional_properties.Set(
          name, ConvertLorgnetteScannerOption(option).ToValue());
    }
  }
  for (const std::string& invalid_name : invalid_option_names) {
    SetOptionResult& result = output.results.emplace_back();
    result.name = invalid_name;
    result.result = OperationResult::kWrongType;
  }
  return output;
}

std::optional<lorgnette::ScannerOption>
TransformOptionSettingToLorgnetteScannerOption(const OptionSetting& setting) {
  lorgnette::ScannerOption option;
  option.set_name(setting.name);

  switch (setting.type) {
    case OptionType::kNone:
    case OptionType::kUnknown:
      option.set_option_type(lorgnette::TYPE_UNKNOWN);
      if (setting.value.has_value()) {
        return std::nullopt;
      }
      break;
    case OptionType::kBool:
      option.set_option_type(lorgnette::TYPE_BOOL);
      if (setting.value.has_value()) {
        if (setting.value->as_boolean.has_value()) {
          option.set_bool_value(*setting.value->as_boolean);
        } else {
          return std::nullopt;
        }
      }
      break;
    case OptionType::kInt: {
      option.set_option_type(lorgnette::TYPE_INT);
      if (setting.value.has_value()) {
        if (setting.value->as_integer.has_value()) {
          option.mutable_int_value()->add_value(*setting.value->as_integer);
        } else if (setting.value->as_integers.has_value()) {
          for (int i : *setting.value->as_integers) {
            option.mutable_int_value()->add_value(i);
          }
        } else if (setting.value->as_number.has_value()) {
          double d = *setting.value->as_number;
          double int_part = 0.0;
          if (d >= std::numeric_limits<int32_t>::min() &&
              d <= std::numeric_limits<int32_t>::max() &&
              std::modf(d, &int_part) == 0.0) {
            option.mutable_int_value()->add_value(
                base::checked_cast<int32_t>(d));
          } else {
            return std::nullopt;
          }
        } else if (setting.value->as_numbers.has_value()) {
          std::vector<int32_t> ints;
          for (double d : *setting.value->as_numbers) {
            double int_part = 0.0;
            if (d >= std::numeric_limits<int32_t>::min() &&
                d <= std::numeric_limits<int32_t>::max() &&
                std::modf(d, &int_part) == 0.0) {
              ints.push_back(base::checked_cast<int32_t>(d));
            } else {
              return std::nullopt;
            }
          }
          for (int i : ints) {
            option.mutable_int_value()->add_value(i);
          }
        } else {
          return std::nullopt;
        }
      }
      break;
    }
    case OptionType::kFixed:
      option.set_option_type(lorgnette::TYPE_FIXED);
      if (setting.value.has_value()) {
        if (setting.value->as_number.has_value()) {
          option.mutable_fixed_value()->add_value(*setting.value->as_number);
        } else if (setting.value->as_numbers.has_value()) {
          for (double d : *setting.value->as_numbers) {
            option.mutable_fixed_value()->add_value(d);
          }
        } else if (setting.value->as_integer.has_value()) {
          option.mutable_fixed_value()->add_value(*setting.value->as_integer);
        } else if (setting.value->as_integers.has_value()) {
          for (int i : *setting.value->as_integers) {
            option.mutable_fixed_value()->add_value(i);
          }
        } else {
          return std::nullopt;
        }
      }
      break;
    case OptionType::kString:
      option.set_option_type(lorgnette::TYPE_STRING);
      if (setting.value.has_value()) {
        if (setting.value->as_string.has_value()) {
          option.set_string_value(*setting.value->as_string);
        } else {
          return std::nullopt;
        }
      }
      break;
    case OptionType::kButton:
      option.set_option_type(lorgnette::TYPE_BUTTON);
      break;
    case OptionType::kGroup:
      option.set_option_type(lorgnette::TYPE_GROUP);
      break;
  }

  return option;
}

OpenScannerResponse ConvertLorgnetteOpenScannerResponse(
    const lorgnette::OpenScannerResponse& input) {
  OpenScannerResponse output;
  output.scanner_id = input.scanner_id().connection_string();
  output.result = ConvertLorgnetteOperationResult(input.result());
  if (input.has_config()) {
    output.scanner_handle = input.config().scanner().token();
    output.options.emplace();
    for (const auto& [name, option] : input.config().options()) {
      output.options->additional_properties.Set(
          name, ConvertLorgnetteScannerOption(option).ToValue());
    }
  }
  return output;
}

ScannerOption ConvertLorgnetteScannerOptionForTesting(
    const lorgnette::ScannerOption& input) {
  CHECK_IS_TEST();
  return ConvertLorgnetteScannerOption(input);
}

OptionConstraint ConvertLorgnetteOptionConstraintForTesting(
    const lorgnette::OptionConstraint& input) {
  CHECK_IS_TEST();
  return ConvertLorgnetteOptionConstraint(input);
}

OptionType ConvertLorgnetteOptionTypeForTesting(
    const lorgnette::OptionType& input) {
  CHECK_IS_TEST();
  return ConvertLorgnetteOptionType(input);
}

OptionUnit ConvertLorgnetteOptionUnitForTesting(
    const lorgnette::OptionUnit& input) {
  CHECK_IS_TEST();
  return ConvertLorgnetteOptionUnit(input);
}

ConstraintType ConvertLorgnetteOptionConstraintTypeForTesting(
    const lorgnette::OptionConstraint_ConstraintType& input) {
  CHECK_IS_TEST();
  return ConvertLorgnetteOptionConstraintType(input);
}

std::optional<ScannerOption::Value> GetLorgnetteOptionValueForTesting(
    const lorgnette::ScannerOption& option) {
  CHECK_IS_TEST();
  return GetLorgnetteOptionValue(option);
}

}  // namespace extensions::api::document_scan
