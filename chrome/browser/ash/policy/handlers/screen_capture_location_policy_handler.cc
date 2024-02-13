// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/screen_capture_location_policy_handler.h"

#include <string>
#include <utility>

#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

const char kGoogleDriveNamePolicyVariableName[] = "${google_drive}";
const char kOneDriveNamePolicyVariableName[] = "${onedrive}";

bool IsValidLocationString(const std::string& str) {
  const size_t google_drive_position =
      str.find(kGoogleDriveNamePolicyVariableName);
  if (google_drive_position != std::string::npos &&
      google_drive_position != 0) {
    return false;
  }
  const size_t onedrive_position = str.find(kOneDriveNamePolicyVariableName);
  if (onedrive_position != std::string::npos && onedrive_position != 0) {
    return false;
  }
  return true;
}

}  // namespace

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

  if (value && !IsValidLocationString(value->GetString())) {
    errors->AddError(policy_name(), IDS_POLICY_VALUE_FORMAT_ERROR,
                     value->GetString());
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

  if (!value || !value->is_string()) {
    return;
  }

  // TODO(b/323146997): Set Screen Capture custom path expanding the policy.
  //  prefs->SetString(ash::kCustomCapturePathPrefName,
  //                  value->GetString());
}

}  // namespace policy
