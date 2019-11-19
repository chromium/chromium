// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_CONFIG_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_CONFIG_H_

#include <string>

#include "base/files/file_path.h"

namespace policy {

// A container keeping all parameters relevant to whether and how enterprise
// enrollment of a device should occur. This configures the behavior of the
// enrollment flow during OOBE, i.e. whether the enrollment screen starts
// automatically, whether the user can skip enrollment, and what domain to
// display as owning the device.
struct EnrollmentConfig {
  // Describes the enrollment mode, i.e. what triggered enrollment.
  enum Mode {
    // Enrollment not applicable.
    MODE_NONE,
    // Manually triggered initial enrollment.
    MODE_MANUAL,
    // Manually triggered re-enrollment.
    MODE_MANUAL_REENROLLMENT,
    // Forced enrollment triggered by local OEM manifest or device requisition,
    // user can't skip.
    MODE_LOCAL_FORCED,
    // Advertised enrollment triggered by local OEM manifest or device
    // requisition, user can skip.
    MODE_LOCAL_ADVERTISED,
    // Server-backed-state-triggered forced enrollment, user can't skip.
    MODE_SERVER_FORCED,
    // Server-backed-state-triggered advertised enrollment, user can skip.
    MODE_SERVER_ADVERTISED,
    // Recover from "spontaneous unenrollment", user can't skip.
    MODE_RECOVERY,
    // Start attestation-based enrollment.
    MODE_ATTESTATION,
    // Start attestation-based enrollment and only uses that.
    MODE_ATTESTATION_LOCAL_FORCED,
    // Server-backed-state-triggered attestation-based enrollment, user can't
    // skip.
    MODE_ATTESTATION_SERVER_FORCED,
    // Forced enrollment triggered as a fallback to attestation re-enrollment,
    // user can't skip.
    MODE_ATTESTATION_MANUAL_FALLBACK,
    // Enrollment for offline demo mode with locally stored policy data.
    MODE_OFFLINE_DEMO,
    // Flow that happens when already enrolled device undergoes version
    // rollback. Enrollment information is preserved during rollback, but
    // some steps have to be repeated as stateful partition was wiped.
    MODE_ENROLLED_ROLLBACK,
    // Server-backed-state-triggered forced initial enrollment, user can't
    // skip.
    MODE_INITIAL_SERVER_FORCED,
    // Server-backed-state-triggered attestation-based initial enrollment,
    // user can't skip.
    MODE_ATTESTATION_INITIAL_SERVER_FORCED,
    // Forced enrollment triggered as a fallback to attestation initial
    // enrollment, user can't skip.
    MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK,

    // Attestation-based enrollment with enrollment token, used in configuration
    // based OOBE.
    MODE_ATTESTATION_ENROLLMENT_TOKEN,
  };

  // An enumeration of authentication mechanisms that can be used for
  // enrollment.
  enum AuthMechanism {
    // Interactive authentication.
    AUTH_MECHANISM_INTERACTIVE,
    // Automatic authentication relying on the attestation process.
    AUTH_MECHANISM_ATTESTATION,
    // Let the system determine the best mechanism (typically the one
    // that requires the least user interaction).
    AUTH_MECHANISM_BEST_AVAILABLE,
  };

  EnrollmentConfig();
  EnrollmentConfig(const EnrollmentConfig& config);
  ~EnrollmentConfig();

  // Whether enrollment should be triggered.
  bool should_enroll() const {
    return should_enroll_with_attestation() || should_enroll_interactively();
  }

  // Whether attestation enrollment should be triggered.
  bool should_enroll_with_attestation() const {
    return auth_mechanism != AUTH_MECHANISM_INTERACTIVE;
  }

  // Whether interactive enrollment should be triggered.
  bool should_enroll_interactively() const { return mode != MODE_NONE; }

  // Whether we fell back into manual enrollment.
  bool is_manual_fallback() const {
    return mode == MODE_ATTESTATION_MANUAL_FALLBACK ||
           mode == MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK;
  }

  // Whether enrollment is forced. The user can't skip the enrollment step
  // during OOBE if this returns true.
  bool is_forced() const {
    return mode == MODE_LOCAL_FORCED || mode == MODE_SERVER_FORCED ||
           mode == MODE_INITIAL_SERVER_FORCED || mode == MODE_RECOVERY ||
           is_attestation_forced() || is_manual_fallback();
  }

  // Whether attestation-based enrollment is forced. The user can't skip
  // the enrollment step during OOBE if this returns true.
  bool is_attestation_forced() const {
    return auth_mechanism == AUTH_MECHANISM_ATTESTATION;
  }

  // Whether this configuration is in attestation mode per server request.
  bool is_mode_attestation_server() const {
    return mode == MODE_ATTESTATION_SERVER_FORCED ||
           mode == MODE_ATTESTATION_INITIAL_SERVER_FORCED;
  }

  // Whether this configuration is in attestation mode.
  bool is_mode_attestation() const {
    return mode == MODE_ATTESTATION || mode == MODE_ATTESTATION_LOCAL_FORCED ||
           mode == MODE_ATTESTATION_ENROLLMENT_TOKEN ||
           is_mode_attestation_server();
  }

  // Whether this configuration is in OAuth mode.
  bool is_mode_oauth() const {
    return mode != MODE_NONE && !is_mode_attestation();
  }

  // Whether state keys request should be skipped.
  // Skipping the request is allowed only for offline demo mode. Offline demo
  // mode setup ensures that online validation of state keys is not required in
  // that case.
  bool skip_state_keys_request() const { return mode == MODE_OFFLINE_DEMO; }

  // Indicates the enrollment flow variant to trigger during OOBE.
  Mode mode = MODE_NONE;

  // The domain to enroll the device to, if applicable. If this is not set, the
  // device may be enrolled to any domain. Note that for the case where the
  // device is not already locked to a certain domain, this value is used for
  // display purposes only and the server makes the final decision on which
  // domain the device should be enrolled with. If the device is already locked
  // to a domain, policy validation during enrollment will verify the domains
  // match.
  std::string management_domain;

  // The realm the device is joined to (if managed by AD).
  std::string management_realm;

  // Enrollment token to use for authentication (for USB-enrollment).
  std::string enrollment_token;

  // Is a license packaged with device or not.
  bool is_license_packaged_with_device = false;

  // The authentication mechanism to use.
  // TODO(drcrash): Change to best available once ZTE is everywhere.
  AuthMechanism auth_mechanism = AUTH_MECHANISM_INTERACTIVE;

  // The path for the device policy blob data for the offline demo mode. This
  // should be empty and never used for other modes.
  base::FilePath offline_policy_path;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_ENROLLMENT_CONFIG_H_
