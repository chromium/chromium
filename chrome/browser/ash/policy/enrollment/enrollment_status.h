// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_

#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#endif

namespace policy {

// Describes the result of an enrollment operation, including the relevant error
// codes received from the involved components.  Note that the component error
// codes only convey useful information in case |status_| points towards a
// problem in that specific component.
class EnrollmentStatus {
 public:
  // Enrollment status codes.  Do not change the numeric ids or the meaning of
  // the existing codes to preserve the interpretability of old logfiles.
  enum Status {
    SUCCESS = 0,                     // Enrollment succeeded.
    NO_STATE_KEYS = 1,               // Server-backed state keys unavailable.
    REGISTRATION_FAILED = 2,         // DM registration failed.
    REGISTRATION_BAD_MODE = 3,       // Bad device mode.
    ROBOT_AUTH_FETCH_FAILED = 4,     // API OAuth2 auth code failure.
    ROBOT_REFRESH_FETCH_FAILED = 5,  // API OAuth2 refresh token failure.
    ROBOT_REFRESH_STORE_FAILED = 6,  // Failed to store API OAuth2 token.
    POLICY_FETCH_FAILED = 7,         // DM policy fetch failed.
    VALIDATION_FAILED = 8,           // Policy validation failed.
    LOCK_ERROR = 9,                  // Cryptohome failed to lock device.
    /* LOCK_TIMEOUT = 10, */         // Unused: Timeout while waiting for the
                                     // lock.
    /* LOCK_WRONG_USER = 11, */      // Unused: Locked to different domain.
    STORE_ERROR = 12,                // Failed to store the policy.
    /* STORE_TOKEN_AND_ID_FAILED = 13, */  // Unused: Failed to store DM token
                                           // and device ID.
    ATTRIBUTE_UPDATE_FAILED = 14,          // Device attribute update failed.
    REGISTRATION_CERT_FETCH_FAILED = 15,   // Cannot obtain registration cert.
    NO_MACHINE_IDENTIFICATION = 16,        // Machine model or serial missing.
    ACTIVE_DIRECTORY_POLICY_FETCH_FAILED = 17,  // Failed to fetch Active
                                                // Directory policy via
                                                // authpolicyd.
    DM_TOKEN_STORE_FAILED = 18,             // Failed to store DM token into the
                                            // local state.
    /* LICENSE_REQUEST_FAILED = 19, */      // Unused: Failed to get available
                                            // license types.
    /* OFFLINE_POLICY_LOAD_FAILED = 20, */  // Deprecated: Failed to load the
                                            // policy data for the offline demo
                                            // mode.
    /* OFFLINE_POLICY_DECODING_FAILED = 21, */  // Deprecated: Failed when the
                                                // policy data fails to be
                                                // decoded.
    // Device policy would block dev mode but the
    // kEnterpriseEnrollmentFailOnBlockDevMode flag was given.
    MAY_NOT_BLOCK_DEV_MODE = 22,
  };

  // Helpers for constructing errors for relevant cases.
  static EnrollmentStatus ForStatus(Status status);
  static EnrollmentStatus ForRegistrationError(
      DeviceManagementStatus client_status);
  static EnrollmentStatus ForFetchError(DeviceManagementStatus client_status);
  static EnrollmentStatus ForRobotAuthFetchError(
      DeviceManagementStatus client_status);
  static EnrollmentStatus ForValidationError(
      CloudPolicyValidatorBase::Status validation_status);
  static EnrollmentStatus ForStoreError(
      CloudPolicyStore::Status store_error,
      CloudPolicyValidatorBase::Status validation_status);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  static EnrollmentStatus ForLockError(
      ash::InstallAttributes::LockResult lock_status);
#endif

  Status status() const { return status_; }
  DeviceManagementStatus client_status() const { return client_status_; }
  CloudPolicyStore::Status store_status() const { return store_status_; }
  CloudPolicyValidatorBase::Status validation_status() const {
    return validation_status_;
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::InstallAttributes::LockResult lock_status() const {
    return lock_status_;
  }
#endif

 private:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EnrollmentStatus(Status status,
                   DeviceManagementStatus client_status,
                   CloudPolicyStore::Status store_status,
                   CloudPolicyValidatorBase::Status validation_status,
                   ash::InstallAttributes::LockResult lock_status);
#else
  EnrollmentStatus(Status status,
                   DeviceManagementStatus client_status,
                   CloudPolicyStore::Status store_status,
                   CloudPolicyValidatorBase::Status validation_status);
#endif

  // Helper to implement the For* functions above without them knowing about the
  // current platform.
  static EnrollmentStatus CreateEnrollmentStatusWithoutLockError(
      Status status,
      DeviceManagementStatus client_status,
      CloudPolicyStore::Status store_status,
      CloudPolicyValidatorBase::Status validation_status);

  Status status_;
  DeviceManagementStatus client_status_;
  CloudPolicyStore::Status store_status_;
  CloudPolicyValidatorBase::Status validation_status_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::InstallAttributes::LockResult lock_status_;
#endif
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_
