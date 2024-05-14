// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"

#include <string>

#include "base/notreached.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace policy {

const char kDeviceStateManagementDomain[] = "management_domain";
const char kDeviceStateMode[] = "device_mode";
const char kDeviceStateDisabledMessage[] = "disabled_message";
const char kDeviceStatePackagedLicense[] = "packaged_license";
const char kDeviceStateLicenseType[] = "license_type";
const char kDeviceStateAssignedUpgradeType[] = "assigned_upgrade_type";

// Modes for a device after initial state determination.
const char kDeviceStateInitialModeEnrollmentEnforced[] = "enrollment-enforced";
const char kDeviceStateInitialModeEnrollmentZeroTouch[] =
    "enrollment-zero-touch";
const char kDeviceStateInitialModeTokenEnrollment[] = "token-enrollment";
// Modes for a device after secondary state determination (FRE).
const char kDeviceStateRestoreModeReEnrollmentRequested[] =
    "re-enrollment-requested";
const char kDeviceStateRestoreModeReEnrollmentEnforced[] =
    "re-enrollment-enforced";
const char kDeviceStateRestoreModeReEnrollmentZeroTouch[] =
    "re-enrollment-zero-touch";
// License types of a device after initial state determination.
const char kDeviceStateLicenseTypeEnterprise[] = "enterprise";
const char kDeviceStateLicenseTypeEducation[] = "education";
const char kDeviceStateLicenseTypeTerminal[] = "terminal";
// Modes for a device after either initial or secondary state determination.
const char kDeviceStateModeDisabled[] = "disabled";

// Assigned upgrades for a device after initial state determination.
const char kDeviceStateAssignedUpgradeTypeChromeEnterprise[] =
    "enterprise";
const char kDeviceStateAssignedUpgradeTypeKiosk[] = "kiosk";

DeviceStateMode GetDeviceStateMode() {
  const std::string* device_state_mode =
      g_browser_process->local_state()
          ->GetDict(prefs::kServerBackedDeviceState)
          .FindString(kDeviceStateMode);
  if (!device_state_mode || device_state_mode->empty())
    return RESTORE_MODE_NONE;
  if (*device_state_mode == kDeviceStateRestoreModeReEnrollmentRequested)
    return RESTORE_MODE_REENROLLMENT_REQUESTED;
  if (*device_state_mode == kDeviceStateRestoreModeReEnrollmentEnforced)
    return RESTORE_MODE_REENROLLMENT_ENFORCED;
  if (*device_state_mode == kDeviceStateModeDisabled)
    return RESTORE_MODE_DISABLED;
  if (*device_state_mode == kDeviceStateRestoreModeReEnrollmentZeroTouch)
    return RESTORE_MODE_REENROLLMENT_ZERO_TOUCH;
  if (*device_state_mode == kDeviceStateInitialModeEnrollmentEnforced)
    return INITIAL_MODE_ENROLLMENT_ENFORCED;
  if (*device_state_mode == kDeviceStateInitialModeEnrollmentZeroTouch)
    return INITIAL_MODE_ENROLLMENT_ZERO_TOUCH;
  if (*device_state_mode == kDeviceStateInitialModeTokenEnrollment) {
    return INITIAL_MODE_ENROLLMENT_TOKEN_ENROLLMENT;
  }

  NOTREACHED_IN_MIGRATION();
  return RESTORE_MODE_NONE;
}

}  // namespace policy
