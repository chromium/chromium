// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/os_color_mode_policy_handler.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {
namespace {

constexpr const char kOsColorModeLight[] = "light";
constexpr const char kOsColorModeDark[] = "dark";
constexpr const char kOsColorModeAuto[] = "auto";
constexpr const char* kAllowedValues[] = {kOsColorModeLight, kOsColorModeDark,
                                          kOsColorModeAuto};

}  // namespace

OsColorModePolicyHandler::OsColorModePolicyHandler() = default;

OsColorModePolicyHandler::~OsColorModePolicyHandler() = default;

bool OsColorModePolicyHandler::CheckPolicySettings(const PolicyMap& policies,
                                                   PolicyErrorMap* errors) {
  const base::Value* value = policies.GetValueUnsafe(key::kOsColorMode);

  if (!value)
    return true;

  if (!value->is_string()) {
    errors->AddError(key::kOsColorMode, IDS_POLICY_TYPE_ERROR,
                     base::Value::GetTypeName(base::Value::Type::STRING));
    return false;
  }

  if (!base::Contains(kAllowedValues, value->GetString())) {
    errors->AddError(key::kOsColorMode, IDS_POLICY_OUT_OF_RANGE_ERROR,
                     value->GetString());
    return false;
  }

  return true;
}

void OsColorModePolicyHandler::ApplyPolicySettings(const PolicyMap& policies,
                                                   PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(key::kOsColorMode, base::Value::Type::STRING);

  if (!value)
    return;

  const std::string& string_value = value->GetString();

  if (string_value == kOsColorModeAuto) {
    prefs->SetInteger(ash::prefs::kDarkModeScheduleType,
                      static_cast<int>(ash::ScheduleType::kSunsetToSunrise));
  } else {
    prefs->SetInteger(ash::prefs::kDarkModeScheduleType,
                      static_cast<int>(ash::ScheduleType::kNone));
    prefs->SetBoolean(ash::prefs::kDarkModeEnabled,
                      string_value == kOsColorModeDark);
  }
}

}  // namespace policy
