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

}  // namespace chromeos
