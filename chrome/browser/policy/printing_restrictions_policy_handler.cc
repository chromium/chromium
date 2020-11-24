// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/printing_restrictions_policy_handler.h"

#include "build/chromeos_buildflags.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

template <class Mode>
PrintingEnumPolicyHandler<Mode>::PrintingEnumPolicyHandler(
    const char* policy_name,
    const char* pref_name,
    const base::flat_map<std::string, Mode>& policy_value_to_mode)
    : TypeCheckingPolicyHandler(policy_name, base::Value::Type::STRING),
      policy_name_(policy_name),
      pref_name_(pref_name),
      policy_value_to_mode_(policy_value_to_mode) {}

template <class Mode>
PrintingEnumPolicyHandler<Mode>::~PrintingEnumPolicyHandler() = default;

template <class Mode>
bool PrintingEnumPolicyHandler<Mode>::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

template <class Mode>
void PrintingEnumPolicyHandler<Mode>::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  Mode value;
  if (GetValue(policies, nullptr, &value)) {
    prefs->SetInteger(pref_name_, static_cast<int>(value));
  }
}

template <class Mode>
bool PrintingEnumPolicyHandler<Mode>::GetValue(const PolicyMap& policies,
                                               PolicyErrorMap* errors,
                                               Mode* result) {
  const base::Value* value;
  if (CheckAndGetValue(policies, errors, &value) && value) {
    base::Optional<Mode> mode;
    auto it = policy_value_to_mode_.find(value->GetString());
    if (it != policy_value_to_mode_.end())
      mode = it->second;
    if (mode.has_value()) {
      if (result)
        *result = mode.value();
      return true;
    }
    if (errors)
      errors->AddError(policy_name_, IDS_POLICY_VALUE_FORMAT_ERROR);
  }
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
PrintingAllowedColorModesPolicyHandler::PrintingAllowedColorModesPolicyHandler()
    : PrintingEnumPolicyHandler<printing::ColorModeRestriction>(
          key::kPrintingAllowedColorModes,
          prefs::kPrintingAllowedColorModes,
          {
              {"any", printing::ColorModeRestriction::kUnset},
              {"monochrome", printing::ColorModeRestriction::kMonochrome},
              {"color", printing::ColorModeRestriction::kColor},
          }) {}

PrintingAllowedColorModesPolicyHandler::
    ~PrintingAllowedColorModesPolicyHandler() = default;

PrintingColorDefaultPolicyHandler::PrintingColorDefaultPolicyHandler()
    : PrintingEnumPolicyHandler<printing::ColorModeRestriction>(
          key::kPrintingColorDefault,
          prefs::kPrintingColorDefault,
          {
              {"monochrome", printing::ColorModeRestriction::kMonochrome},
              {"color", printing::ColorModeRestriction::kColor},
          }) {}

PrintingColorDefaultPolicyHandler::~PrintingColorDefaultPolicyHandler() =
    default;

PrintingAllowedDuplexModesPolicyHandler::
    PrintingAllowedDuplexModesPolicyHandler()
    : PrintingEnumPolicyHandler<printing::DuplexModeRestriction>(
          key::kPrintingAllowedDuplexModes,
          prefs::kPrintingAllowedDuplexModes,
          {
              {"any", printing::DuplexModeRestriction::kUnset},
              {"simplex", printing::DuplexModeRestriction::kSimplex},
              {"duplex", printing::DuplexModeRestriction::kDuplex},
          }) {}

PrintingAllowedDuplexModesPolicyHandler::
    ~PrintingAllowedDuplexModesPolicyHandler() = default;

PrintingDuplexDefaultPolicyHandler::PrintingDuplexDefaultPolicyHandler()
    : PrintingEnumPolicyHandler<printing::DuplexModeRestriction>(
          key::kPrintingDuplexDefault,
          prefs::kPrintingDuplexDefault,
          {
              {"simplex", printing::DuplexModeRestriction::kSimplex},
              {"long-edge", printing::DuplexModeRestriction::kLongEdge},
              {"short-edge", printing::DuplexModeRestriction::kShortEdge},
          }) {}

PrintingDuplexDefaultPolicyHandler::~PrintingDuplexDefaultPolicyHandler() =
    default;

PrintingAllowedPinModesPolicyHandler::PrintingAllowedPinModesPolicyHandler()
    : PrintingEnumPolicyHandler<printing::PinModeRestriction>(
          key::kPrintingAllowedPinModes,
          prefs::kPrintingAllowedPinModes,
          {
              {"any", printing::PinModeRestriction::kUnset},
              {"pin", printing::PinModeRestriction::kPin},
              {"no_pin", printing::PinModeRestriction::kNoPin},
          }) {}

PrintingAllowedPinModesPolicyHandler::~PrintingAllowedPinModesPolicyHandler() =
    default;

PrintingPinDefaultPolicyHandler::PrintingPinDefaultPolicyHandler()
    : PrintingEnumPolicyHandler<printing::PinModeRestriction>(
          key::kPrintingPinDefault,
          prefs::kPrintingPinDefault,
          {
              {"pin", printing::PinModeRestriction::kPin},
              {"no_pin", printing::PinModeRestriction::kNoPin},
          }) {}

PrintingPinDefaultPolicyHandler::~PrintingPinDefaultPolicyHandler() = default;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

PrintingAllowedBackgroundGraphicsModesPolicyHandler::
    PrintingAllowedBackgroundGraphicsModesPolicyHandler()
    : PrintingEnumPolicyHandler<printing::BackgroundGraphicsModeRestriction>(
          key::kPrintingAllowedBackgroundGraphicsModes,
          prefs::kPrintingAllowedBackgroundGraphicsModes,
          {
              {"any", printing::BackgroundGraphicsModeRestriction::kUnset},
              {"enabled",
               printing::BackgroundGraphicsModeRestriction::kEnabled},
              {"disabled",
               printing::BackgroundGraphicsModeRestriction::kDisabled},
          }) {}

PrintingAllowedBackgroundGraphicsModesPolicyHandler::
    ~PrintingAllowedBackgroundGraphicsModesPolicyHandler() = default;

PrintingBackgroundGraphicsDefaultPolicyHandler::
    PrintingBackgroundGraphicsDefaultPolicyHandler()
    : PrintingEnumPolicyHandler<printing::BackgroundGraphicsModeRestriction>(
          key::kPrintingBackgroundGraphicsDefault,
          prefs::kPrintingBackgroundGraphicsDefault,
          {
              {"enabled",
               printing::BackgroundGraphicsModeRestriction::kEnabled},
              {"disabled",
               printing::BackgroundGraphicsModeRestriction::kDisabled},
          }) {}

PrintingBackgroundGraphicsDefaultPolicyHandler::
    ~PrintingBackgroundGraphicsDefaultPolicyHandler() = default;

PrintingPaperSizeDefaultPolicyHandler::PrintingPaperSizeDefaultPolicyHandler()
    : TypeCheckingPolicyHandler(key::kPrintingPaperSizeDefault,
                                base::Value::Type::DICTIONARY) {}

PrintingPaperSizeDefaultPolicyHandler::
    ~PrintingPaperSizeDefaultPolicyHandler() = default;

bool PrintingPaperSizeDefaultPolicyHandler::CheckIntSubkey(
    const base::Value* dict,
    const std::string& key,
    PolicyErrorMap* errors) {
  const base::Value* value = dict->FindKey(key);
  if (!value) {
    if (errors) {
      errors->AddError(policy_name(), key, IDS_POLICY_NOT_SPECIFIED_ERROR);
    }
    return false;
  }
  if (!value->is_int()) {
    if (errors) {
      errors->AddError(policy_name(), key, IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::INTEGER));
    }
    return false;
  }
  return true;
}

bool PrintingPaperSizeDefaultPolicyHandler::GetValue(
    const PolicyMap& policies,
    PolicyErrorMap* errors,
    const base::Value** result) {
  const base::Value* value;
  if (!CheckAndGetValue(policies, errors, &value)) {
    if (result)
      *result = nullptr;
    return false;
  }
  if (result)
    *result = value;

  if (!value)
    return true;

  const base::Value* name = value->FindKey(printing::kPaperSizeName);
  if (!name) {
    if (errors)
      errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }
  if (!name->is_string()) {
    if (errors) {
      errors->AddError(policy_name(), printing::kPaperSizeName,
                       IDS_POLICY_TYPE_ERROR,
                       base::Value::GetTypeName(base::Value::Type::STRING));
    }
    return false;
  }
  bool custom_option_specified =
      (name->GetString() == printing::kPaperSizeNameCustomOption);

  bool custom_size_property_found = false;
  const base::Value* custom_size =
      value->FindKey(printing::kPaperSizeCustomSize);
  if (custom_size) {
    if (!custom_size->is_dict()) {
      if (errors) {
        errors->AddError(
            policy_name(), printing::kPaperSizeCustomSize,
            IDS_POLICY_TYPE_ERROR,
            base::Value::GetTypeName(base::Value::Type::DICTIONARY));
      }
      return false;
    }
    if (!CheckIntSubkey(custom_size, printing::kPaperSizeWidth, errors) ||
        !CheckIntSubkey(custom_size, printing::kPaperSizeHeight, errors)) {
      return false;
    }
    custom_size_property_found = true;
  }

  if (custom_option_specified != custom_size_property_found) {
    if (errors)
      errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  return true;
}

bool PrintingPaperSizeDefaultPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  return GetValue(policies, errors, nullptr);
}

void PrintingPaperSizeDefaultPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value;
  if (GetValue(policies, nullptr, &value) && value) {
    prefs->SetValue(prefs::kPrintingPaperSizeDefault, value->Clone());
  }
}

}  // namespace policy
