// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_

namespace policy {

// Dictionary key constants for prefs::kServerBackedDeviceState.
extern const char kDeviceStateManagementDomain[];
extern const char kDeviceStateMode[];
extern const char kDeviceStateDisabledMessage[];
extern const char kDeviceStatePackagedLicense[];

// String constants used to persist the restorative action in the
// kDeviceStateMode dictionary entry.
extern const char kDeviceStateRestoreModeReEnrollmentRequested[];
extern const char kDeviceStateRestoreModeReEnrollmentEnforced[];
extern const char kDeviceStateRestoreModeDisabled[];
extern const char kDeviceStateRestoreModeReEnrollmentZeroTouch[];
// The following constants are for an initial action but we do still use
// the same dictionary entry.
extern const char kDeviceStateInitialModeEnrollmentEnforced[];
extern const char kDeviceStateInitialModeEnrollmentZeroTouch[];

// Mode that a device needs to start in.
enum DeviceStateMode {
  // No state restoration.
  RESTORE_MODE_NONE = 0,
  // Enterprise re-enrollment requested, but user may skip.
  RESTORE_MODE_REENROLLMENT_REQUESTED = 1,
  // Enterprise re-enrollment is enforced and cannot be skipped.
  RESTORE_MODE_REENROLLMENT_ENFORCED = 2,
  // The device has been disabled by its owner. The device will show a warning
  // screen and prevent the user from proceeding further.
  RESTORE_MODE_DISABLED = 3,
  // Enterprise re-enrollment is enforced using Zero-Touch and cannot be
  // skipped.
  RESTORE_MODE_REENROLLMENT_ZERO_TOUCH = 4,
  // Enterprise initial enrollment is enforced and cannot be skipped.
  INITIAL_MODE_ENROLLMENT_ENFORCED = 5,
  // Enterprise initial enrollment is enforced and cannot be skipped.
  INITIAL_MODE_ENROLLMENT_ZERO_TOUCH = 6,
};

// Parses the contents of the kDeviceStateMode dictionary entry and
// returns it as a DeviceStateMode.
DeviceStateMode GetDeviceStateMode();

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_
