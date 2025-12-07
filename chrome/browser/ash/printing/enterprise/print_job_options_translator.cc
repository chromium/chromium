// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/enterprise/print_job_options_translator.h"

#include <optional>
#include <string>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "chrome/browser/ash/printing/enterprise/print_job_options.pb.h"

namespace chromeos {

// Print job options names.
const char kMediaSize[] = "media_size";
const char kMediaType[] = "media_type";
const char kDuplex[] = "duplex";
const char kColor[] = "color";
const char kDpi[] = "dpi";
const char kQuality[] = "quality";
const char kPrintAsImage[] = "print_as_image";

const char kDefaultValue[] = "default_value";
const char kAllowedValues[] = "allowed_values";

// Size field names.
const char kWidth[] = "width";
const char kHeight[] = "height";

// Dpi field names.
const char kHorizontal[] = "horizontal";
const char kVertical[] = "vertical";

namespace {

// Helper functions to convert JSON to proto.

std::optional<Size> SizeFromValue(const base::Value& size) {
  if (!size.is_dict()) {
    return std::nullopt;
  }

  const std::optional<int> width = size.GetDict().FindInt(kWidth);
  const std::optional<int> height = size.GetDict().FindInt(kHeight);

  Size result;
  if (width) {
    result.set_width(width.value());
  }
  if (height) {
    result.set_height(height.value());
  }

  return result;
}

std::optional<DPI> DpiFromValue(const base::Value& dpi) {
  if (!dpi.is_dict()) {
    return std::nullopt;
  }

  const std::optional<int> horizontal = dpi.GetDict().FindInt(kHorizontal);
  const std::optional<int> vertical = dpi.GetDict().FindInt(kVertical);

  DPI result;
  if (horizontal) {
    result.set_horizontal(horizontal.value());
  }
  if (vertical) {
    result.set_vertical(vertical.value());
  }

  return result;
}

template <typename OptionType, typename OptionValueType>
OptionType OptionFromDict(
    const base::Value::Dict& option_dict,
    base::RepeatingCallback<std::optional<OptionValueType>(const base::Value&)>
        get_option_value) {
  OptionType option;

  const base::Value* default_value = option_dict.Find(kDefaultValue);
  const base::Value::List* allowed_values =
      option_dict.FindList(kAllowedValues);

  // Protobuf generates different setter mechanisms (either `set_X()` or
  // `mutable_X()`) based on the field type. `set_X()` is generated for numeric
  // types and enums, `mutable_X()` should be available for other types.
  constexpr bool use_value_setter =
      std::is_integral_v<OptionValueType> || std::is_enum_v<OptionValueType>;

  if (default_value) {
    std::optional<OptionValueType> default_value_opt =
        get_option_value.Run(*default_value);
    if (default_value_opt) {
      if constexpr (use_value_setter) {
        option.set_default_value(default_value_opt.value());
      } else {
        *option.mutable_default_value() = default_value_opt.value();
      }
    }
  }

  if (allowed_values) {
    for (const base::Value& allowed_value : *allowed_values) {
      std::optional<OptionValueType> allowed_value_opt =
          get_option_value.Run(allowed_value);
      if (allowed_value_opt) {
        if constexpr (use_value_setter) {
          option.add_allowed_values(allowed_value_opt.value());
        } else {
          *option.add_allowed_values() = allowed_value_opt.value();
        }
      }
    }
  }

  return option;
}

// Helper functions to convert proto to ChromeOS Printer class.

std::optional<Printer::Size> SizeFromProtoSize(const Size& size) {
  if (!size.has_height()) {
    LOG(WARNING) << "`size` proto is malformed: height value is not set";
    return std::nullopt;
  } else if (!size.has_width()) {
    LOG(WARNING) << "`size` proto is malformed: width value is not set";
    return std::nullopt;
  }

  return Printer::Size{.width = size.width(), .height = size.height()};
}

Printer::DuplexType DuplexFromProtoDuplex(const DuplexType& duplex_type) {
  switch (duplex_type) {
    case DuplexType::DUPLEX_ONE_SIDED:
      return Printer::DuplexType::kOneSided;
    case DuplexType::DUPLEX_SHORT_EDGE:
      return Printer::DuplexType::kShortEdge;
    case DuplexType::DUPLEX_LONG_EDGE:
      return Printer::DuplexType::kLongEdge;
    default:
      return Printer::DuplexType::kUnknownDuplexType;
  }
}

std::optional<Printer::Dpi> DpiFromProtoDpi(const DPI& dpi) {
  if (!dpi.has_horizontal()) {
    LOG(WARNING) << "`dpi` proto is malformed: `horizontal` field is not set";
    return std::nullopt;
  } else if (!dpi.has_vertical()) {
    LOG(WARNING) << "`dpi` proto is malformed: `vertical` field is not set";
    return std::nullopt;
  }

  return Printer::Dpi{.horizontal = dpi.horizontal(),
                      .vertical = dpi.vertical()};
}

Printer::QualityType QualityFromProtoQuality(const QualityType& quality_type) {
  switch (quality_type) {
    case QualityType::QUALITY_DRAFT:
      return Printer::QualityType::kDraft;
    case QualityType::QUALITY_NORMAL:
      return Printer::QualityType::kNormal;
    case QualityType::QUALITY_HIGH:
      return Printer::QualityType::kHigh;
    default:
      return Printer::QualityType::kUnknownQualityType;
  }
}

// Use this function for field conversions that always succeed.
template <typename OptionValueType, typename ProtoValueType, typename InputType>
Printer::PrintOption<OptionValueType> ParsePrinterJobOption(
    const InputType& input,
    base::RepeatingCallback<OptionValueType(const ProtoValueType&)> convert) {
  Printer::PrintOption<OptionValueType> result;

  if (input.has_default_value()) {
    result.default_value = convert.Run(input.default_value());
  }

  result.allowed_values.reserve(input.allowed_values().size());
  // Range-based loop isn't convenient here, since generated Protobuf getters
  // for enums return array of ints, not array of enums. This way we would need
  // to explicitly convert these values to enum values.
  for (int i = 0; i < input.allowed_values_size(); ++i) {
    result.allowed_values.push_back(convert.Run(input.allowed_values(i)));
  }

  return result;
}

template <typename ProtoValueType, typename InputType>
Printer::PrintOption<ProtoValueType> ParsePrinterJobOption(
    const InputType& input) {
  return ParsePrinterJobOption(
      input,
      base::BindRepeating([](const ProtoValueType& value) { return value; }));
}

// Use this function for field conversions that may fail.
template <typename OptionValueType, typename ProtoValueType, typename InputType>
std::optional<Printer::PrintOption<OptionValueType>> ParsePrinterJobOption(
    const InputType& input,
    base::RepeatingCallback<
        std::optional<OptionValueType>(const ProtoValueType&)> try_convert) {
  Printer::PrintOption<OptionValueType> result;

  if (input.has_default_value()) {
    std::optional<OptionValueType> default_value =
        try_convert.Run(input.default_value());
    if (!default_value) {
      return std::nullopt;
    }
    result.default_value = default_value;
  }

  result.allowed_values.reserve(input.allowed_values().size());
  // Range-based loop isn't convenient here, since generated Protobuf getters
  // for enums return array of ints, not array of enums. This way we would need
  // to explicitly convert these values to enum values.
  for (int i = 0; i < input.allowed_values_size(); ++i) {
    std::optional<OptionValueType> allowed_value =
        try_convert.Run(input.allowed_values(i));
    if (!allowed_value) {
      return std::nullopt;
    }
    result.allowed_values.push_back(allowed_value.value());
  }

  return result;
}

}  // namespace

PrintJobOptions ManagedPrintOptionsProtoFromDict(
    const base::Value::Dict& print_job_options) {
  PrintJobOptions result;
  const base::Value::Dict* media_size = print_job_options.FindDict(kMediaSize);
  const base::Value::Dict* media_type = print_job_options.FindDict(kMediaType);
  const base::Value::Dict* duplex = print_job_options.FindDict(kDuplex);
  const base::Value::Dict* color = print_job_options.FindDict(kColor);
  const base::Value::Dict* dpi = print_job_options.FindDict(kDpi);
  const base::Value::Dict* quality = print_job_options.FindDict(kQuality);
  const base::Value::Dict* print_as_image =
      print_job_options.FindDict(kPrintAsImage);

  if (media_size) {
    *result.mutable_media_size() = OptionFromDict<SizeOption>(
        *media_size, base::BindRepeating(&SizeFromValue));
  }
  if (media_type) {
    *result.mutable_media_type() = OptionFromDict<StringOption>(
        *media_type, base::BindRepeating([](const base::Value& data) {
          return data.is_string() ? std::optional<std::string>(data.GetString())
                                  : std::nullopt;
        }));
  }
  if (duplex) {
    *result.mutable_duplex() = OptionFromDict<DuplexOption>(
        *duplex, base::BindRepeating(
                     [](const base::Value& data) -> std::optional<DuplexType> {
                       if (data.is_int() && DuplexType_IsValid(data.GetInt())) {
                         return static_cast<DuplexType>(data.GetInt());
                       }
                       return std::nullopt;
                     }));
  }
  if (color) {
    *result.mutable_color() = OptionFromDict<BoolOption>(
        *color, base::BindRepeating([](const base::Value& data) {
          return data.is_bool() ? std::optional<bool>(data.GetBool())
                                : std::nullopt;
        }));
  }
  if (dpi) {
    *result.mutable_dpi() =
        OptionFromDict<DPIOption>(*dpi, base::BindRepeating(&DpiFromValue));
  }
  if (quality) {
    *result.mutable_quality() = OptionFromDict<QualityOption>(
        *quality,
        base::BindRepeating(
            [](const base::Value& data) -> std::optional<QualityType> {
              if (data.is_int() && QualityType_IsValid(data.GetInt())) {
                return static_cast<QualityType>(data.GetInt());
              }
              return std::nullopt;
            }));
  }
  if (print_as_image) {
    *result.mutable_print_as_image() = OptionFromDict<BoolOption>(
        *print_as_image, base::BindRepeating([](const base::Value& data) {
          return data.is_bool() ? std::optional<bool>(data.GetBool())
                                : std::nullopt;
        }));
  }

  return result;
}

std::optional<Printer::ManagedPrintOptions>
ChromeOsPrintOptionsFromManagedPrintOptions(
    const PrintJobOptions& print_job_options) {
  Printer::ManagedPrintOptions result;

  if (print_job_options.has_media_size()) {
    auto media_size = ParsePrinterJobOption<Printer::Size>(
        print_job_options.media_size(),
        base::BindRepeating(&SizeFromProtoSize));
    if (!media_size) {
      return std::nullopt;
    }
    result.media_size = media_size.value();
  }

  if (print_job_options.has_media_type()) {
    result.media_type =
        ParsePrinterJobOption<std::string>(print_job_options.media_type());
  }

  if (print_job_options.has_duplex()) {
    result.duplex = ParsePrinterJobOption<Printer::DuplexType>(
        print_job_options.duplex(),
        base::BindRepeating(&DuplexFromProtoDuplex));
  }

  if (print_job_options.has_color()) {
    result.color = ParsePrinterJobOption<bool>(print_job_options.color());
  }

  if (print_job_options.has_quality()) {
    result.quality = ParsePrinterJobOption<Printer::QualityType>(
        print_job_options.quality(),
        base::BindRepeating(&QualityFromProtoQuality));
  }

  if (print_job_options.has_dpi()) {
    auto dpi = ParsePrinterJobOption<Printer::Dpi>(
        print_job_options.dpi(), base::BindRepeating(&DpiFromProtoDpi));
    if (!dpi) {
      return std::nullopt;
    }
    result.dpi = dpi.value();
  }

  if (print_job_options.has_print_as_image()) {
    result.print_as_image =
        ParsePrinterJobOption<bool>(print_job_options.print_as_image());
  }

  return result;
}

}  // namespace chromeos
