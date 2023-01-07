// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_color_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/re2/src/re2/re2.h"
#include "third_party/skia/include/core/SkColor.h"

namespace {

// Check if the given string is a valid hex color.
bool IsValidHexColor(const std::string& input) {
  // A valid hex color must be of form "#RRGGBB" ("#" is optional).
  // Support for "RGB" may be added in the future.
  return RE2::FullMatch(input, "^#?[0-9a-fA-F]{6}$");
}

// Convert valid hex string to corresponding SkColor.
SkColor HexToSkColor(const std::string& hex_color) {
  DCHECK(!hex_color.empty());
  DCHECK(IsValidHexColor(hex_color));

  const int kHexColorLength = 6;
  int color;
  // A valid hex color may or may not have "#" as the first character.
  base::HexStringToInt(
      hex_color.substr(hex_color[0] == '#' ? 1 : 0, kHexColorLength), &color);
  return SkColorSetA(color, SK_AlphaOPAQUE);
}

}  // namespace

ThemeColorPolicyHandler::ThemeColorPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kBrowserThemeColor,
                                base::Value::Type::STRING) {}

ThemeColorPolicyHandler::~ThemeColorPolicyHandler() = default;

bool ThemeColorPolicyHandler::CheckPolicySettings(
    const policy::PolicyMap& policies,
    policy::PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value))
    return false;

  if (value && !IsValidHexColor(value->GetString())) {
    errors->AddError(policy_name(), IDS_POLICY_HEX_COLOR_ERROR,
                     value->GetString());
    return false;
  }
  return true;
}

void ThemeColorPolicyHandler::ApplyPolicySettings(
    const policy::PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value =
      policies.GetValue(policy_name(), base::Value::Type::STRING);
  if (!value)
    return;

  prefs->SetInteger(prefs::kPolicyThemeColor, HexToSkColor(value->GetString()));
}
