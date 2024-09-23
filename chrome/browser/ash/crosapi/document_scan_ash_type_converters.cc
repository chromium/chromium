// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/document_scan_ash_type_converters.h"

#include <cstdint>

#include "base/notreached.h"
#include "base/strings/strcat.h"

namespace mojo {

template <>
struct TypeConverter<crosapi::mojom::ScannerInfo_ConnectionType,
                     lorgnette::ConnectionType> {
  static crosapi::mojom::ScannerInfo_ConnectionType Convert(
      lorgnette::ConnectionType input) {
    using ConnectionType = crosapi::mojom::ScannerInfo_ConnectionType;

    switch (input) {
      default:
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case lorgnette::ConnectionType::CONNECTION_UNSPECIFIED:
        return ConnectionType::kUnspecified;
      case lorgnette::ConnectionType::CONNECTION_USB:
        return ConnectionType::kUsb;
      case lorgnette::ConnectionType::CONNECTION_NETWORK:
        return ConnectionType::kNetwork;
    }
  }
};

template <>
struct TypeConverter<crosapi::mojom::ScannerInfoPtr, lorgnette::ScannerInfo> {
  static crosapi::mojom::ScannerInfoPtr Convert(
      const lorgnette::ScannerInfo& input) {
    auto output = crosapi::mojom::ScannerInfo::New();
    output->id = input.name();
    output->display_name = input.display_name();
    output->manufacturer = input.manufacturer();
    output->model = input.model();
    output->device_uuid = input.device_uuid();
    output->connection_type =
        ConvertTo<crosapi::mojom::ScannerInfo_ConnectionType>(
            input.connection_type());
    output->secure = input.secure();
    output->image_formats.reserve(input.image_format_size());
    for (const std::string& format : input.image_format()) {
      output->image_formats.emplace_back(format);
    }
    output->protocol_type = input.protocol_type();
    return output;
  }
};

template <>
struct TypeConverter<crosapi::mojom::OptionType, lorgnette::OptionType> {
  static crosapi::mojom::OptionType Convert(lorgnette::OptionType input) {
    switch (input) {
      default:
        // Default case included to cover protobuf sentinel values.
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case lorgnette::TYPE_UNKNOWN:
        return crosapi::mojom::OptionType::kUnknown;
      case lorgnette::TYPE_BOOL:
        return crosapi::mojom::OptionType::kBool;
      case lorgnette::TYPE_INT:
        return crosapi::mojom::OptionType::kInt;
      case lorgnette::TYPE_FIXED:
        return crosapi::mojom::OptionType::kFixed;
      case lorgnette::TYPE_STRING:
        return crosapi::mojom::OptionType::kString;
      case lorgnette::TYPE_BUTTON:
        return crosapi::mojom::OptionType::kButton;
      case lorgnette::TYPE_GROUP:
        return crosapi::mojom::OptionType::kGroup;
    }
  }
};
crosapi::mojom::OptionType ConvertForTesting(lorgnette::OptionType input) {
  return ConvertTo<crosapi::mojom::OptionType>(input);
}

template <>
struct TypeConverter<lorgnette::OptionType, crosapi::mojom::OptionType> {
  static lorgnette::OptionType Convert(crosapi::mojom::OptionType input) {
    switch (input) {
      case crosapi::mojom::OptionType::kUnknown:
        return lorgnette::TYPE_UNKNOWN;
      case crosapi::mojom::OptionType::kBool:
        return lorgnette::TYPE_BOOL;
      case crosapi::mojom::OptionType::kInt:
        return lorgnette::TYPE_INT;
      case crosapi::mojom::OptionType::kFixed:
        return lorgnette::TYPE_FIXED;
      case crosapi::mojom::OptionType::kString:
        return lorgnette::TYPE_STRING;
      case crosapi::mojom::OptionType::kButton:
        return lorgnette::TYPE_BUTTON;
      case crosapi::mojom::OptionType::kGroup:
        return lorgnette::TYPE_GROUP;
    }
  }
};

template <>
struct TypeConverter<crosapi::mojom::OptionUnit, lorgnette::OptionUnit> {
  static crosapi::mojom::OptionUnit Convert(lorgnette::OptionUnit input) {
    switch (input) {
      default:
        // Default case included to cover protobuf sentinel values.
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case lorgnette::UNIT_NONE:
        return crosapi::mojom::OptionUnit::kUnitless;
      case lorgnette::UNIT_PIXEL:
        return crosapi::mojom::OptionUnit::kPixel;
      case lorgnette::UNIT_BIT:
        return crosapi::mojom::OptionUnit::kBit;
      case lorgnette::UNIT_MM:
        return crosapi::mojom::OptionUnit::kMm;
      case lorgnette::UNIT_DPI:
        return crosapi::mojom::OptionUnit::kDpi;
      case lorgnette::UNIT_PERCENT:
        return crosapi::mojom::OptionUnit::kPercent;
      case lorgnette::UNIT_MICROSECOND:
        return crosapi::mojom::OptionUnit::kMicrosecond;
    }
  }
};
crosapi::mojom::OptionUnit ConvertForTesting(lorgnette::OptionUnit input) {
  return ConvertTo<crosapi::mojom::OptionUnit>(input);
}

template <>
struct TypeConverter<crosapi::mojom::OptionConstraintType,
                     lorgnette::OptionConstraint_ConstraintType> {
  static crosapi::mojom::OptionConstraintType Convert(
      lorgnette::OptionConstraint_ConstraintType input) {
    switch (input) {
      default:
        // Default case included to cover protobuf sentinel values.
        NOTREACHED_IN_MIGRATION();
        [[fallthrough]];
      case lorgnette::OptionConstraint::CONSTRAINT_NONE:
        return crosapi::mojom::OptionConstraintType::kNone;
      case lorgnette::OptionConstraint::CONSTRAINT_INT_RANGE:
        return crosapi::mojom::OptionConstraintType::kIntRange;
      case lorgnette::OptionConstraint::CONSTRAINT_FIXED_RANGE:
        return crosapi::mojom::OptionConstraintType::kFixedRange;
      case lorgnette::OptionConstraint::CONSTRAINT_INT_LIST:
        return crosapi::mojom::OptionConstraintType::kIntList;
      case lorgnette::OptionConstraint::CONSTRAINT_FIXED_LIST:
        return crosapi::mojom::OptionConstraintType::kFixedList;
      case lorgnette::OptionConstraint::CONSTRAINT_STRING_LIST:
        return crosapi::mojom::OptionConstraintType::kStringList;
    }
  }
};
crosapi::mojom::OptionConstraintType ConvertForTesting(  // IN-TEST
    lorgnette::OptionConstraint_ConstraintType input) {
  return ConvertTo<crosapi::mojom::OptionConstraintType>(input);
}

template <>
struct TypeConverter<crosapi::mojom::IntRangePtr,
                     lorgnette::OptionConstraint_IntRange> {
  static crosapi::mojom::IntRangePtr Convert(
      const lorgnette::OptionConstraint_IntRange& input) {
    auto output = crosapi::mojom::IntRange::New();
    output->min = input.min();
    output->max = input.max();
    output->quant = input.quant();
    return output;
  }
};

template <>
struct TypeConverter<crosapi::mojom::FixedRangePtr,
                     lorgnette::OptionConstraint_FixedRange> {
  static crosapi::mojom::FixedRangePtr Convert(
      const lorgnette::OptionConstraint_FixedRange& input) {
    auto output = crosapi::mojom::FixedRange::New();
    output->min = input.min();
    output->max = input.max();
    output->quant = input.quant();
    return output;
  }
};

template <>
struct TypeConverter<crosapi::mojom::OptionConstraintPtr,
                     lorgnette::OptionConstraint> {
  static crosapi::mojom::OptionConstraintPtr Convert(
      const lorgnette::OptionConstraint& input) {
    auto output = crosapi::mojom::OptionConstraint::New();
    output->type = ConvertTo<crosapi::mojom::OptionConstraintType>(
        input.constraint_type());
    // In the protobuf, constraints are represented as several different fields.
    // In mojom, these are mapped to one union for a closer match to the JS API
    // that consumes this.
    //
    // If the protobuf does not have the correct value field set to match its
    // declared type, this will create an output with the same type and an empty
    // constraint.  The empty constraint will always fail to match any value,
    // but will be semantically correct.
    switch (output->type) {
      case crosapi::mojom::OptionConstraintType::kNone:
        // No constraint value.
        break;
      case crosapi::mojom::OptionConstraintType::kIntRange:
        if (!input.has_int_range()) {
          LOG(WARNING) << "OptionConstraint has type INT_RANGE but does not "
                          "contain a valid int_range";
        }
        output->restriction =
            crosapi::mojom::OptionConstraintRestriction::NewIntRange(
                crosapi::mojom::IntRange::From(input.int_range()));
        break;
      case crosapi::mojom::OptionConstraintType::kIntList:
        if (input.valid_int().empty()) {
          LOG(WARNING) << "OptionConstraint has type INT_LIST but does not "
                          "contain a valid valid_int";
        }
        output->restriction =
            crosapi::mojom::OptionConstraintRestriction::NewValidInt(
                std::vector<int32_t>{input.valid_int().begin(),
                                     input.valid_int().end()});
        break;
      case crosapi::mojom::OptionConstraintType::kFixedRange:
        if (!input.has_fixed_range()) {
          LOG(WARNING) << "OptionConstraint has type FIXED_RANGE but does not "
                          "contain a valid fixed_range";
        }
        output->restriction =
            crosapi::mojom::OptionConstraintRestriction::NewFixedRange(
                crosapi::mojom::FixedRange::From(input.fixed_range()));
        break;
      case crosapi::mojom::OptionConstraintType::kFixedList:
        if (input.valid_fixed().empty()) {
          LOG(WARNING) << "OptionConstraint has type FIXED_LIST but does not "
                          "contain a valid valid_fixed";
        }
        output->restriction =
            crosapi::mojom::OptionConstraintRestriction::NewValidFixed(
                std::vector<double>{input.valid_fixed().begin(),
                                    input.valid_fixed().end()});
        break;
      case crosapi::mojom::OptionConstraintType::kStringList:
        if (input.valid_string().empty()) {
          LOG(WARNING) << "OptionConstraint has type STRING_LIST but does not "
                          "contain a valid valid_string";
        }
        output->restriction =
            crosapi::mojom::OptionConstraintRestriction::NewValidString(
                std::vector<std::string>{input.valid_string().begin(),
                                         input.valid_string().end()});
        break;
    }
    return output;
  }
};
crosapi::mojom::OptionConstraintPtr ConvertForTesting(  // IN-TEST
    const lorgnette::OptionConstraint& input) {
  return crosapi::mojom::OptionConstraint::From(input);
}

template <>
struct TypeConverter<crosapi::mojom::ScannerOptionPtr,
                     lorgnette::ScannerOption> {
  static crosapi::mojom::ScannerOptionPtr Convert(
      const lorgnette::ScannerOption& input) {
    auto output = crosapi::mojom::ScannerOption::New();
    output->name = input.name();
    output->title = input.title();
    output->description = input.description();
    output->type = ConvertTo<crosapi::mojom::OptionType>(input.option_type());
    output->unit = ConvertTo<crosapi::mojom::OptionUnit>(input.unit());

    // The value of an option in lorgnette is represented as several separate
    // fields.  These map into a single union for mojom to better match the IDL
    // definition on the JS side.
    //
    // An unset value field is explicitly allowed in the protobuf and
    // means that the option does not currently have a valid value.  If the
    // wrong value field is set in the protobuf (i.e., some field that doesn't
    // match the claimed type), this is treated the same as an unset value.
    //
    // INT and FIXED values are special because they can contain more than one
    // value.  These are mapped to separate fields to match the JS API, but are
    // handled the same way in protobuf.  For these two cases, a single-element
    // array is mapped to the _value field and multiple-element arrays are
    // mapped to the _list field.
    switch (output->type) {
      case crosapi::mojom::OptionType::kUnknown:
      case crosapi::mojom::OptionType::kButton:
      case crosapi::mojom::OptionType::kGroup:
        // No value for these.
        break;
      case crosapi::mojom::OptionType::kBool:
        if (input.has_bool_value()) {
          output->value =
              crosapi::mojom::OptionValue::NewBoolValue(input.bool_value());
        }
        break;
      case crosapi::mojom::OptionType::kInt:
        if (!input.has_int_value()) {
          // Unset value.  Do nothing.
        } else if (input.int_value().value_size() == 1) {
          // Single value.
          output->value = crosapi::mojom::OptionValue::NewIntValue(
              input.int_value().value(0));
        } else {
          // Multiple values.
          output->value = crosapi::mojom::OptionValue::NewIntList(
              std::vector<int32_t>{input.int_value().value().begin(),
                                   input.int_value().value().end()});
        }
        break;
      case crosapi::mojom::OptionType::kFixed:
        if (!input.has_fixed_value()) {
          // Unset value.  Do nothing.
        } else if (input.fixed_value().value_size() == 1) {
          // Single value.
          output->value = crosapi::mojom::OptionValue::NewFixedValue(
              input.fixed_value().value(0));
        } else {
          // Multiple values.
          output->value = crosapi::mojom::OptionValue::NewFixedList(
              std::vector<double>{input.fixed_value().value().begin(),
                                  input.fixed_value().value().end()});
        }
        break;
      case crosapi::mojom::OptionType::kString:
        if (input.has_string_value()) {
          output->value =
              crosapi::mojom::OptionValue::NewStringValue(input.string_value());
        }
        break;
    }

    if (input.has_constraint()) {
      output->constraint =
          crosapi::mojom::OptionConstraint::From(input.constraint());
    }
    output->isDetectable = input.detectable();

    // Configurability represents the combinations of sw_settable+hw_settable.
    // Only one is allowed to be set according to SANE, so there isn't a case
    // for both.
    if (input.sw_settable()) {
      output->configurability =
          crosapi::mojom::OptionConfigurability::kSoftwareConfigurable;
    } else if (input.hw_settable()) {
      output->configurability =
          crosapi::mojom::OptionConfigurability::kHardwareConfigurable;
    } else {
      output->configurability =
          crosapi::mojom::OptionConfigurability::kNotConfigurable;
    }

    output->isAutoSettable = input.auto_settable();
    output->isEmulated = input.emulated();
    output->isActive = input.active();
    output->isAdvanced = input.advanced();
    output->isInternal = false;  // Lorgnette currently does not return internal
                                 // options.
    return output;
  }
};
crosapi::mojom::ScannerOptionPtr ConvertForTesting(  // IN-TEST
    const lorgnette::ScannerOption& input) {
  return crosapi::mojom::ScannerOption::From(input);
}

std::optional<lorgnette::ScannerOption>
TypeConverter<std::optional<lorgnette::ScannerOption>,
              crosapi::mojom::OptionSettingPtr>::
    Convert(const crosapi::mojom::OptionSettingPtr& input) {
  lorgnette::ScannerOption output;
  output.set_name(input->name);
  output.set_option_type(ConvertTo<lorgnette::OptionType>(input->type));

  // Some options are automatically settable in which case a value is not
  // needed.
  if (!input->value) {
    return output;
  }

  // If there is a value, check to make sure it has the correct type.
  switch (input->type) {
    case (crosapi::mojom::OptionType::kBool):
      if (!input->value->is_bool_value()) {
        return std::nullopt;
      }
      output.set_bool_value(input->value->get_bool_value());
      break;
    case (crosapi::mojom::OptionType::kInt):
      // This can represent a single int value or a list.  Check for both.
      if (input->value->is_int_value()) {
        output.mutable_int_value()->add_value(input->value->get_int_value());
      } else if (input->value->is_int_list()) {
        for (auto& value : input->value->get_int_list()) {
          output.mutable_int_value()->add_value(value);
        }
      } else {
        return std::nullopt;
      }
      break;
    case (crosapi::mojom::OptionType::kFixed):
      // This can represent a single fixed value or a list.  Check for both.
      if (input->value->is_fixed_value()) {
        output.mutable_fixed_value()->add_value(
            input->value->get_fixed_value());
      } else if (input->value->is_fixed_list()) {
        for (auto& value : input->value->get_fixed_list()) {
          output.mutable_fixed_value()->add_value(value);
        }
      } else {
        return std::nullopt;
      }
      break;
    case (crosapi::mojom::OptionType::kString):
      if (!input->value->is_string_value()) {
        return std::nullopt;
      }
      output.set_string_value(input->value->get_string_value());
      break;
    case (crosapi::mojom::OptionType::kUnknown):
    case (crosapi::mojom::OptionType::kButton):
    case (crosapi::mojom::OptionType::kGroup):
      // kButton and kGroup don't need to set a value.
      break;
  }

  return output;
}

crosapi::mojom::ScannerOperationResult TypeConverter<
    crosapi::mojom::ScannerOperationResult,
    lorgnette::OperationResult>::Convert(lorgnette::OperationResult input) {
  switch (input) {
    default:
      NOTREACHED_IN_MIGRATION();
      [[fallthrough]];
    case lorgnette::OPERATION_RESULT_UNKNOWN:
      return crosapi::mojom::ScannerOperationResult::kUnknown;
    case lorgnette::OPERATION_RESULT_SUCCESS:
      return crosapi::mojom::ScannerOperationResult::kSuccess;
    case lorgnette::OPERATION_RESULT_UNSUPPORTED:
      return crosapi::mojom::ScannerOperationResult::kUnsupported;
    case lorgnette::OPERATION_RESULT_CANCELLED:
      return crosapi::mojom::ScannerOperationResult::kCancelled;
    case lorgnette::OPERATION_RESULT_DEVICE_BUSY:
      return crosapi::mojom::ScannerOperationResult::kDeviceBusy;
    case lorgnette::OPERATION_RESULT_INVALID:
      return crosapi::mojom::ScannerOperationResult::kInvalid;
    case lorgnette::OPERATION_RESULT_WRONG_TYPE:
      return crosapi::mojom::ScannerOperationResult::kWrongType;
    case lorgnette::OPERATION_RESULT_EOF:
      return crosapi::mojom::ScannerOperationResult::kEndOfData;
    case lorgnette::OPERATION_RESULT_ADF_JAMMED:
      return crosapi::mojom::ScannerOperationResult::kAdfJammed;
    case lorgnette::OPERATION_RESULT_ADF_EMPTY:
      return crosapi::mojom::ScannerOperationResult::kAdfEmpty;
    case lorgnette::OPERATION_RESULT_COVER_OPEN:
      return crosapi::mojom::ScannerOperationResult::kCoverOpen;
    case lorgnette::OPERATION_RESULT_IO_ERROR:
      return crosapi::mojom::ScannerOperationResult::kIoError;
    case lorgnette::OPERATION_RESULT_ACCESS_DENIED:
      return crosapi::mojom::ScannerOperationResult::kAccessDenied;
    case lorgnette::OPERATION_RESULT_NO_MEMORY:
      return crosapi::mojom::ScannerOperationResult::kNoMemory;
    case lorgnette::OPERATION_RESULT_UNREACHABLE:
      return crosapi::mojom::ScannerOperationResult::kDeviceUnreachable;
    case lorgnette::OPERATION_RESULT_MISSING:
      return crosapi::mojom::ScannerOperationResult::kDeviceMissing;
    case lorgnette::OPERATION_RESULT_INTERNAL_ERROR:
      return crosapi::mojom::ScannerOperationResult::kInternalError;
  }
}

crosapi::mojom::GetScannerListResponsePtr
TypeConverter<crosapi::mojom::GetScannerListResponsePtr,
              lorgnette::ListScannersResponse>::
    Convert(const lorgnette::ListScannersResponse& input) {
  auto output = crosapi::mojom::GetScannerListResponse::New();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  output->scanners.reserve(input.scanners().size());
  for (const auto& scanner : input.scanners()) {
    output->scanners.emplace_back(crosapi::mojom::ScannerInfo::From(scanner));
  }
  return output;
}

crosapi::mojom::OpenScannerResponsePtr
TypeConverter<crosapi::mojom::OpenScannerResponsePtr,
              lorgnette::OpenScannerResponse>::
    Convert(const lorgnette::OpenScannerResponse& input) {
  auto output = crosapi::mojom::OpenScannerResponse::New();
  output->scanner_id = input.scanner_id().connection_string();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  if (!input.has_config()) {
    return output;
  }

  const lorgnette::ScannerConfig& config = input.config();
  output->scanner_handle = config.scanner().token();
  output->options =
      base::flat_map<std::string, crosapi::mojom::ScannerOptionPtr>();
  output->options->reserve(config.options().size());
  for (const auto& [name, option] : config.options()) {
    output->options->try_emplace(name,
                                 crosapi::mojom::ScannerOption::From(option));
  }
  return output;
}

crosapi::mojom::CloseScannerResponsePtr
TypeConverter<crosapi::mojom::CloseScannerResponsePtr,
              lorgnette::CloseScannerResponse>::
    Convert(const lorgnette::CloseScannerResponse& input) {
  auto output = crosapi::mojom::CloseScannerResponse::New();
  output->scanner_handle = input.scanner().token();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  return output;
}

crosapi::mojom::StartPreparedScanResponsePtr
TypeConverter<crosapi::mojom::StartPreparedScanResponsePtr,
              lorgnette::StartPreparedScanResponse>::
    Convert(const lorgnette::StartPreparedScanResponse& input) {
  auto output = crosapi::mojom::StartPreparedScanResponse::New();
  output->scanner_handle = input.scanner().token();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  if (input.has_job_handle()) {
    output->job_handle = input.job_handle().token();
  }
  return output;
}

crosapi::mojom::ReadScanDataResponsePtr
TypeConverter<crosapi::mojom::ReadScanDataResponsePtr,
              lorgnette::ReadScanDataResponse>::
    Convert(const lorgnette::ReadScanDataResponse& input) {
  auto output = crosapi::mojom::ReadScanDataResponse::New();
  output->job_handle = input.job_handle().token();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());
  if (input.has_data()) {
    output->data.emplace(input.data().begin(), input.data().end());
  }
  if (input.has_estimated_completion()) {
    output->estimated_completion = input.estimated_completion();
  }
  return output;
}

crosapi::mojom::SetOptionsResponsePtr TypeConverter<
    crosapi::mojom::SetOptionsResponsePtr,
    lorgnette::SetOptionsResponse>::Convert(const lorgnette::SetOptionsResponse&
                                                input) {
  auto output = crosapi::mojom::SetOptionsResponse::New();
  output->scanner_handle = input.scanner().token();
  // Populate output.results.
  for (const auto& [input_name, input_result] : input.results()) {
    auto this_result = crosapi::mojom::SetOptionResult::New();
    this_result->name = input_name;
    this_result->result =
        ConvertTo<crosapi::mojom::ScannerOperationResult>(input_result);
    output->results.emplace_back(std::move(this_result));
  }
  // Populate output.options.
  const lorgnette::ScannerConfig& config = input.config();
  output->options.emplace();
  output->options->reserve(config.options().size());
  for (const auto& [name, option] : config.options()) {
    output->options->try_emplace(name,
                                 crosapi::mojom::ScannerOption::From(option));
  }

  return output;
}

crosapi::mojom::GetOptionGroupsResponsePtr
TypeConverter<crosapi::mojom::GetOptionGroupsResponsePtr,
              lorgnette::GetCurrentConfigResponse>::
    Convert(const lorgnette::GetCurrentConfigResponse& input) {
  auto output = crosapi::mojom::GetOptionGroupsResponse::New();
  output->scanner_handle = input.scanner().token();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());

  if (output->result == crosapi::mojom::ScannerOperationResult::kSuccess) {
    output->groups.emplace();
    output->groups->reserve(input.config().option_groups_size());
    // Grab the OptionGroup from the ScannerConfig.
    for (const lorgnette::OptionGroup& group : input.config().option_groups()) {
      auto output_group = crosapi::mojom::OptionGroup::New();
      output_group->title = group.title();
      output_group->members = std::vector<std::string>(group.members().begin(),
                                                       group.members().end());
      output->groups->emplace_back(std::move(output_group));
    }
  }

  return output;
}

crosapi::mojom::CancelScanResponsePtr TypeConverter<
    crosapi::mojom::CancelScanResponsePtr,
    lorgnette::CancelScanResponse>::Convert(const lorgnette::CancelScanResponse&
                                                input) {
  auto output = crosapi::mojom::CancelScanResponse::New();
  output->job_handle = input.job_handle().token();
  output->result =
      ConvertTo<crosapi::mojom::ScannerOperationResult>(input.result());

  return output;
}

}  // namespace mojo
