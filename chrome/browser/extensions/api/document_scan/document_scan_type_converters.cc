// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/document_scan/document_scan_type_converters.h"

namespace mojo {

namespace document_scan = extensions::api::document_scan;
namespace mojom = crosapi::mojom;

document_scan::OperationResult
TypeConverter<document_scan::OperationResult, mojom::ScannerOperationResult>::
    Convert(mojom::ScannerOperationResult input) {
  switch (input) {
    case mojom::ScannerOperationResult::kUnknown:
      return document_scan::OperationResult::kUnknown;
    case mojom::ScannerOperationResult::kSuccess:
      return document_scan::OperationResult::kSuccess;
    case mojom::ScannerOperationResult::kUnsupported:
      return document_scan::OperationResult::kUnsupported;
    case mojom::ScannerOperationResult::kCancelled:
      return document_scan::OperationResult::kCancelled;
    case mojom::ScannerOperationResult::kDeviceBusy:
      return document_scan::OperationResult::kDeviceBusy;
    case mojom::ScannerOperationResult::kInvalid:
      return document_scan::OperationResult::kInvalid;
    case mojom::ScannerOperationResult::kWrongType:
      return document_scan::OperationResult::kWrongType;
    case mojom::ScannerOperationResult::kEndOfData:
      return document_scan::OperationResult::kEof;
    case mojom::ScannerOperationResult::kAdfJammed:
      return document_scan::OperationResult::kAdfJammed;
    case mojom::ScannerOperationResult::kAdfEmpty:
      return document_scan::OperationResult::kAdfEmpty;
    case mojom::ScannerOperationResult::kCoverOpen:
      return document_scan::OperationResult::kCoverOpen;
    case mojom::ScannerOperationResult::kIoError:
      return document_scan::OperationResult::kIoError;
    case mojom::ScannerOperationResult::kAccessDenied:
      return document_scan::OperationResult::kAccessDenied;
    case mojom::ScannerOperationResult::kNoMemory:
      return document_scan::OperationResult::kNoMemory;
    case mojom::ScannerOperationResult::kDeviceUnreachable:
      return document_scan::OperationResult::kUnreachable;
    case mojom::ScannerOperationResult::kDeviceMissing:
      return document_scan::OperationResult::kMissing;
    case mojom::ScannerOperationResult::kInternalError:
      return document_scan::OperationResult::kInternalError;
  }
}

template <>
struct TypeConverter<document_scan::ConnectionType,
                     mojom::ScannerInfo_ConnectionType> {
  static document_scan::ConnectionType Convert(
      mojom::ScannerInfo_ConnectionType input) {
    switch (input) {
      case mojom::ScannerInfo_ConnectionType::kUnspecified:
        return document_scan::ConnectionType::kUnspecified;
      case mojom::ScannerInfo_ConnectionType::kUsb:
        return document_scan::ConnectionType::kUsb;
      case mojom::ScannerInfo_ConnectionType::kNetwork:
        return document_scan::ConnectionType::kNetwork;
    }
  }
};

template <>
struct TypeConverter<document_scan::OptionType, mojom::OptionType> {
  static document_scan::OptionType Convert(mojom::OptionType input) {
    switch (input) {
      case mojom::OptionType::kUnknown:
        return document_scan::OptionType::kUnknown;
      case mojom::OptionType::kBool:
        return document_scan::OptionType::kBool;
      case mojom::OptionType::kInt:
        return document_scan::OptionType::kInt;
      case mojom::OptionType::kFixed:
        return document_scan::OptionType::kFixed;
      case mojom::OptionType::kString:
        return document_scan::OptionType::kString;
      case mojom::OptionType::kButton:
        return document_scan::OptionType::kButton;
      case mojom::OptionType::kGroup:
        return document_scan::OptionType::kGroup;
    }
  }
};
document_scan::OptionType ConvertForTesting(mojom::OptionType input) {
  return ConvertTo<document_scan::OptionType>(input);
}

template <>
struct TypeConverter<document_scan::OptionUnit, mojom::OptionUnit> {
  static document_scan::OptionUnit Convert(mojom::OptionUnit input) {
    switch (input) {
      case mojom::OptionUnit::kUnitless:
        return document_scan::OptionUnit::kUnitless;
      case mojom::OptionUnit::kPixel:
        return document_scan::OptionUnit::kPixel;
      case mojom::OptionUnit::kBit:
        return document_scan::OptionUnit::kBit;
      case mojom::OptionUnit::kMm:
        return document_scan::OptionUnit::kMm;
      case mojom::OptionUnit::kDpi:
        return document_scan::OptionUnit::kDpi;
      case mojom::OptionUnit::kPercent:
        return document_scan::OptionUnit::kPercent;
      case mojom::OptionUnit::kMicrosecond:
        return document_scan::OptionUnit::kMicrosecond;
    }
  }
};
document_scan::OptionUnit ConvertForTesting(mojom::OptionUnit input) {
  return ConvertTo<document_scan::OptionUnit>(input);
}

template <>
struct TypeConverter<document_scan::ConstraintType,
                     mojom::OptionConstraintType> {
  static document_scan::ConstraintType Convert(
      mojom::OptionConstraintType input) {
    switch (input) {
      case mojom::OptionConstraintType::kNone:
        return document_scan::ConstraintType::kNone;
      case mojom::OptionConstraintType::kIntRange:
        return document_scan::ConstraintType::kIntRange;
      case mojom::OptionConstraintType::kFixedRange:
        return document_scan::ConstraintType::kFixedRange;
      case mojom::OptionConstraintType::kIntList:
        return document_scan::ConstraintType::kIntList;
      case mojom::OptionConstraintType::kFixedList:
        return document_scan::ConstraintType::kFixedList;
      case mojom::OptionConstraintType::kStringList:
        return document_scan::ConstraintType::kStringList;
    }
  }
};
document_scan::ConstraintType ConvertForTesting(  // IN-TEST
    mojom::OptionConstraintType input) {
  return ConvertTo<document_scan::ConstraintType>(input);
}

template <>
struct TypeConverter<document_scan::Configurability,
                     mojom::OptionConfigurability> {
  static document_scan::Configurability Convert(
      mojom::OptionConfigurability input) {
    switch (input) {
      case mojom::OptionConfigurability::kNotConfigurable:
        return document_scan::Configurability::kNotConfigurable;
      case mojom::OptionConfigurability::kSoftwareConfigurable:
        return document_scan::Configurability::kSoftwareConfigurable;
      case mojom::OptionConfigurability::kHardwareConfigurable:
        return document_scan::Configurability::kHardwareConfigurable;
    }
  }
};
document_scan::Configurability ConvertForTesting(  // IN-TEST
    mojom::OptionConfigurability input) {
  return ConvertTo<document_scan::Configurability>(input);
}

template <>
struct TypeConverter<document_scan::OptionConstraint,
                     mojom::OptionConstraintPtr> {
  static document_scan::OptionConstraint Convert(
      const mojom::OptionConstraintPtr& input) {
    document_scan::OptionConstraint output;
    if (input.is_null()) {
      return output;
    }

    output.type = ConvertTo<document_scan::ConstraintType>(input->type);
    switch (input->type) {
      case mojom::OptionConstraintType::kNone:
        break;
      case mojom::OptionConstraintType::kIntList: {
        output.list = document_scan::OptionConstraint::List();
        output.list->as_integers =
            std::vector<int32_t>{input->restriction->get_valid_int().begin(),
                                 input->restriction->get_valid_int().end()};
        break;
      }
      case mojom::OptionConstraintType::kFixedList: {
        output.list = document_scan::OptionConstraint::List();
        output.list->as_numbers =
            std::vector<double>{input->restriction->get_valid_fixed().begin(),
                                input->restriction->get_valid_fixed().end()};
        break;
      }
      case mojom::OptionConstraintType::kStringList: {
        output.list = document_scan::OptionConstraint::List();
        output.list->as_strings = std::vector<std::string>{
            input->restriction->get_valid_string().begin(),
            input->restriction->get_valid_string().end()};
        break;
      }
      case mojom::OptionConstraintType::kIntRange: {
        auto& input_range = input->restriction->get_int_range();
        output.min = document_scan::OptionConstraint::Min::FromValue(
            base::Value(input_range->min));
        output.max = document_scan::OptionConstraint::Max::FromValue(
            base::Value(input_range->max));
        output.quant = document_scan::OptionConstraint::Quant::FromValue(
            base::Value(input_range->quant));
        break;
      }
      case mojom::OptionConstraintType::kFixedRange: {
        auto& input_range = input->restriction->get_fixed_range();
        output.min = document_scan::OptionConstraint::Min::FromValue(
            base::Value(input_range->min));
        output.max = document_scan::OptionConstraint::Max::FromValue(
            base::Value(input_range->max));
        output.quant = document_scan::OptionConstraint::Quant::FromValue(
            base::Value(input_range->quant));
        break;
      }
    }
    return output;
  }
};
document_scan::OptionConstraint ConvertForTesting(  // IN-TEST
    const mojom::OptionConstraintPtr& input) {
  return input.To<document_scan::OptionConstraint>();
}

template <>
struct TypeConverter<document_scan::ScannerOption::Value,
                     mojom::OptionValuePtr> {
  static document_scan::ScannerOption::Value Convert(
      const mojom::OptionValuePtr& input) {
    document_scan::ScannerOption::Value output;
    if (input.is_null()) {
      return output;
    }
    switch (input->which()) {
      // Bool maps to a boolean.
      case mojom::OptionValue::Tag::kBoolValue:
        return *document_scan::ScannerOption::Value::FromValue(
            base::Value(input->get_bool_value()));
      // Single int maps to a long.
      case mojom::OptionValue::Tag::kIntValue:
        return *document_scan::ScannerOption::Value::FromValue(
            base::Value(input->get_int_value()));
      // Single fixed maps to a double.
      case mojom::OptionValue::Tag::kFixedValue:
        return *document_scan::ScannerOption::Value::FromValue(
            base::Value(input->get_fixed_value()));
      // String maps to a DOMString.
      case mojom::OptionValue::Tag::kStringValue:
        return *document_scan::ScannerOption::Value::FromValue(
            base::Value(input->get_string_value()));
      // List of ints maps to long[].
      case mojom::OptionValue::Tag::kIntList: {
        document_scan::ScannerOption::Value list;
        list.as_integers = std::vector<int32_t>{input->get_int_list().begin(),
                                                input->get_int_list().end()};
        return list;
      }
      // List of fixed maps to double[].
      case mojom::OptionValue::Tag::kFixedList: {
        document_scan::ScannerOption::Value list;
        list.as_numbers = std::vector<double>{input->get_fixed_list().begin(),
                                              input->get_fixed_list().end()};
        return list;
      }
    }
    return output;
  }
};
document_scan::ScannerOption::Value ConvertForTesting(  // IN-TEST
    const mojom::OptionValuePtr& input) {
  return input.To<document_scan::ScannerOption::Value>();
}

template <>
struct TypeConverter<document_scan::ScannerOption, mojom::ScannerOptionPtr> {
  static document_scan::ScannerOption Convert(
      const mojom::ScannerOptionPtr& input) {
    document_scan::ScannerOption output;
    output.name = input->name;
    output.title = input->title;
    output.description = input->description;
    output.type = ConvertTo<document_scan::OptionType>(input->type);
    output.unit = ConvertTo<document_scan::OptionUnit>(input->unit);
    if (input->value) {
      output.value = input->value.To<document_scan::ScannerOption::Value>();
    }
    if (input->constraint) {
      output.constraint =
          input->constraint.To<document_scan::OptionConstraint>();
    }
    output.is_detectable = input->isDetectable;
    output.configurability =
        ConvertTo<document_scan::Configurability>(input->configurability);
    output.is_auto_settable = input->isAutoSettable;
    output.is_emulated = input->isEmulated;
    output.is_active = input->isActive;
    output.is_advanced = input->isAdvanced;
    output.is_internal = input->isInternal;
    return output;
  }
};
document_scan::ScannerOption ConvertForTesting(  // IN-TEST
    const mojom::ScannerOptionPtr& input) {
  return input.To<document_scan::ScannerOption>();
}

crosapi::mojom::ScannerEnumFilterPtr
TypeConverter<crosapi::mojom::ScannerEnumFilterPtr,
              extensions::api::document_scan::DeviceFilter>::
    Convert(const extensions::api::document_scan::DeviceFilter& input) {
  auto output = crosapi::mojom::ScannerEnumFilter::New();
  output->local = input.local.value_or(false);
  output->secure = input.secure.value_or(false);
  return output;
}

extensions::api::document_scan::GetScannerListResponse
TypeConverter<extensions::api::document_scan::GetScannerListResponse,
              crosapi::mojom::GetScannerListResponsePtr>::
    Convert(const crosapi::mojom::GetScannerListResponsePtr& input) {
  document_scan::GetScannerListResponse output;
  output.result = ConvertTo<document_scan::OperationResult>(input->result);
  for (const mojom::ScannerInfoPtr& scanner_in : input->scanners) {
    document_scan::ScannerInfo& scanner_out = output.scanners.emplace_back();
    scanner_out.scanner_id = scanner_in->id;
    scanner_out.name = scanner_in->display_name;
    scanner_out.manufacturer = scanner_in->manufacturer;
    scanner_out.model = scanner_in->model;
    scanner_out.device_uuid = scanner_in->device_uuid;
    scanner_out.connection_type =
        ConvertTo<document_scan::ConnectionType>(scanner_in->connection_type);
    scanner_out.secure = scanner_in->secure;
    scanner_out.image_formats = scanner_in->image_formats;
  }
  return output;
}

extensions::api::document_scan::OpenScannerResponse
TypeConverter<extensions::api::document_scan::OpenScannerResponse,
              crosapi::mojom::OpenScannerResponsePtr>::
    Convert(const crosapi::mojom::OpenScannerResponsePtr& input) {
  document_scan::OpenScannerResponse output;
  output.scanner_id = input->scanner_id;
  output.result = ConvertTo<document_scan::OperationResult>(input->result);
  if (input->scanner_handle) {
    output.scanner_handle = *input->scanner_handle;
  }
  if (input->options) {
    output.options = document_scan::OpenScannerResponse::Options();
    for (const auto& [name, option] : *input->options) {
      output.options->additional_properties.Set(
          name, option.To<document_scan::ScannerOption>().ToValue());
    }
  }
  return output;
}

extensions::api::document_scan::CloseScannerResponse
TypeConverter<extensions::api::document_scan::CloseScannerResponse,
              crosapi::mojom::CloseScannerResponsePtr>::
    Convert(const crosapi::mojom::CloseScannerResponsePtr& input) {
  document_scan::CloseScannerResponse output;
  output.scanner_handle = input->scanner_handle;
  output.result = ConvertTo<document_scan::OperationResult>(input->result);
  return output;
}

}  // namespace mojo
