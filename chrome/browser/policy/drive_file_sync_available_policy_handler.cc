// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/drive_file_sync_available_policy_handler.h"

#include <string>

#include "base/containers/contains.h"
#include "base/values.h"
#include "components/drive/drive_pref_names.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/strings/grit/components_strings.h"

namespace policy {

namespace {

constexpr const char kDisabled[] = "disabled";
constexpr const char kVisible[] = "visible";
constexpr const char* kAllowedValues[] = {kDisabled, kVisible};

}  // namespace

DriveFileSyncAvailablePolicyHandler::DriveFileSyncAvailablePolicyHandler()
    : TypeCheckingPolicyHandler(key::kDriveFileSyncAvailable,
                                base::Value::Type::STRING) {}
DriveFileSyncAvailablePolicyHandler::~DriveFileSyncAvailablePolicyHandler() =
    default;

bool DriveFileSyncAvailablePolicyHandler::CheckPolicySettings(
    const PolicyMap& policies,
    PolicyErrorMap* errors) {
  const base::Value* value =
      policies.GetValueUnsafe(key::kDriveFileSyncAvailable);
  if (!value) {
    return true;
  }

  if (!base::Contains(kAllowedValues, value->GetString())) {
    errors->AddError(key::kDriveFileSyncAvailable,
                     IDS_POLICY_OUT_OF_RANGE_ERROR, value->GetString());
    return false;
  }

  return true;
}

void DriveFileSyncAvailablePolicyHandler::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs) {
  const base::Value* value = policies.GetValue(key::kDriveFileSyncAvailable,
                                               base::Value::Type::STRING);

  if (!value) {
    return;
  }

  if (value->GetString() == kDisabled) {
    prefs->SetBoolean(drive::prefs::kDriveFsBulkPinningVisible, false);
    prefs->SetBoolean(drive::prefs::kDriveFsBulkPinningEnabled, false);
    return;
  }

  prefs->SetBoolean(drive::prefs::kDriveFsBulkPinningVisible, true);
}

}  // namespace policy
