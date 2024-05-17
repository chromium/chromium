// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"

#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/skyvault/file_location_utils.h"
#include "chrome/common/chrome_features.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

ScreenCaptureLocationPolicyHandler::ScreenCaptureLocationPolicyHandler()
    : TypeCheckingPolicyHandler(policy::key::kScreenCaptureLocation,
                                base::Value::Type::STRING) {}

ScreenCaptureLocationPolicyHandler::~ScreenCaptureLocationPolicyHandler() =
    default;

bool ScreenCaptureLocationPolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, errors, &value)) {
    return false;
  }

  if (value &&
      !policy::local_user_files::IsValidLocationString(value->GetString())) {
    errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR);
    return false;
  }

  return true;
}

void ScreenCaptureLocationPolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = nullptr;
  if (!CheckAndGetValue(policies, nullptr, &value)) {
    return;
  }

  if (!value || !value->is_string() ||
      !policy::local_user_files::IsValidLocationString(value->GetString())) {
    return;
  }

  if (base::FeatureList::IsEnabled(features::kSkyVault)) {
    const std::string str = value->GetString();
    prefs->SetString(ash::prefs::kCaptureModePolicySavePath, str);
  } else {
    VLOG(1) << "SkyVault not enabled, ignoring ScreenCaptureLocation policy";
  }
}

}  // namespace policy
