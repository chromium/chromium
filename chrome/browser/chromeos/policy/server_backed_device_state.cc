// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/server_backed_device_state.h"

#include <string>

#include "base/logging.h"
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

const char kDeviceStateRestoreModeReEnrollmentRequested[] =
    "re-enrollment-requested";
const char kDeviceStateRestoreModeReEnrollmentEnforced[] =
    "re-enrollment-enforced";
const char kDeviceStateRestoreModeDisabled[] = "disabled";
const char kDeviceStateRestoreModeReEnrollmentZeroTouch[] =
    "re-enrollment-zero-touch";
const char kDeviceStateInitialModeEnrollmentEnforced[] = "enrollment-enforced";
const char kDeviceStateInitialModeEnrollmentZeroTouch[] =
    "enrollment-zero-touch";

DeviceStateMode GetDeviceStateMode() {
  std::string device_state_mode;
  g_browser_process->local_state()
      ->GetDictionary(prefs::kServerBackedDeviceState)
      ->GetString(kDeviceStateMode, &device_state_mode);
  if (device_state_mode.empty())
    return RESTORE_MODE_NONE;
  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentRequested)
    return RESTORE_MODE_REENROLLMENT_REQUESTED;
  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentEnforced)
    return RESTORE_MODE_REENROLLMENT_ENFORCED;
  if (device_state_mode == kDeviceStateRestoreModeDisabled)
    return RESTORE_MODE_DISABLED;
  if (device_state_mode == kDeviceStateRestoreModeReEnrollmentZeroTouch)
    return RESTORE_MODE_REENROLLMENT_ZERO_TOUCH;
  if (device_state_mode == kDeviceStateInitialModeEnrollmentEnforced)
    return INITIAL_MODE_ENROLLMENT_ENFORCED;
  if (device_state_mode == kDeviceStateInitialModeEnrollmentZeroTouch)
    return INITIAL_MODE_ENROLLMENT_ZERO_TOUCH;

  NOTREACHED();
  return RESTORE_MODE_NONE;
}

}  // namespace policy
