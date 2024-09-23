// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_
#define CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_

#include <ostream>

#include "chromeos/ash/components/dbus/constants/attestation_constants.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_validator.h"

namespace policy {

// Describes the result of an enrollment operation, including the relevant error
// codes received from the involved components. Note that the component error
// codes only convey useful information in case `code_` points towards a
// problem in that specific component.
class EnrollmentStatus {
 public:
  // Enrollment status codes. Do not change the numeric ids or the meaning of
  // the existing codes to preserve the interpretability of old logfiles.
  enum class Code {
    kSuccess = 0,                       // Enrollment succeeded.
    kNoStateKeys = 1,                   // Server-backed state keys unavailable.
    kRegistrationFailed = 2,            // DM registration failed.
    kRegistrationBadMode = 3,           // Bad device mode.
    kRobotAuthFetchFailed = 4,          // API OAuth2 auth code failure.
    kRobotRefreshFetchFailed = 5,       // API OAuth2 refresh token failure.
    kRobotRefreshStoreFailed = 6,       // Failed to store API OAuth2 token.
    kPolicyFetchFailed = 7,             // DM policy fetch failed.
    kValidationFailed = 8,              // Policy validation failed.
    kLockError = 9,                     // Cryptohome failed to lock device.
    /* kLockTimeout = 10, */            // Unused: Timeout while waiting for the
                                        // lock.
    /* kLockWrongUser = 11, */          // Unused: Locked to different domain.
    kStoreError = 12,                   // Failed to store the policy.
    /* kStoreTokenAndIdFailed = 13, */  // Unused: Failed to store DM token
                                        // and device ID.
    kAttributeUpdateFailed = 14,        // Device attribute update failed.
    kRegistrationCertFetchFailed = 15,  // Cannot obtain registration cert.
    kNoMachineIdentification = 16,      // Machine model or serial missing.
    /* kActiveDirectoryPolicyFetchFailed = 17, */  // Unused since ChromeAD
                                                   // deprecated.
    kDmTokenStoreFailed = 18,             // Failed to store DM token into the
                                          // local state.
    /* kLicenseRequestFailed = 19, */     // Unused: Failed to get available
                                          // license types.
    /* kOfflinePolicyLoadFailed = 20, */  // Deprecated: Failed to load the
                                          // policy data for the offline demo
                                          // mode.
    /* kOfflinePolicyDecodingFailed = 21, */  // Deprecated: Failed when the
                                              // policy data fails to be
                                              // decoded.
    // Device policy would block dev mode but the
    // kEnterpriseEnrollmentFailOnBlockDevMode flag was given.
    kMayNotBlockDevMode = 22,
  };

  // Helpers for constructing errors for relevant cases.
  static EnrollmentStatus ForEnrollmentCode(Code code);
  static EnrollmentStatus ForAttestationError(
      ash::attestation::AttestationStatus attestation_status);
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
  static EnrollmentStatus ForLockError(
      ash::InstallAttributes::LockResult lock_status);

  Code enrollment_code() const { return enrollment_code_; }
  DeviceManagementStatus client_status() const { return client_status_; }
  CloudPolicyStore::Status store_status() const { return store_status_; }
  CloudPolicyValidatorBase::Status validation_status() const {
    return validation_status_;
  }
  ash::InstallAttributes::LockResult lock_status() const {
    return lock_status_;
  }
  ash::attestation::AttestationStatus attestation_status() const {
    return attestation_status_;
  }

  friend bool operator==(const EnrollmentStatus&,
                         const EnrollmentStatus&) = default;

 private:
  Code enrollment_code_ = EnrollmentStatus::Code::kSuccess;
  DeviceManagementStatus client_status_ =
      DeviceManagementStatus::DM_STATUS_SUCCESS;
  CloudPolicyStore::Status store_status_ = CloudPolicyStore::Status::STATUS_OK;
  CloudPolicyValidatorBase::Status validation_status_ =
      CloudPolicyValidatorBase::Status::VALIDATION_OK;
  ash::InstallAttributes::LockResult lock_status_ =
      ash::InstallAttributes::LockResult::LOCK_SUCCESS;
  ash::attestation::AttestationStatus attestation_status_ =
      ash::attestation::ATTESTATION_SUCCESS;

  EnrollmentStatus();
};

std::ostream& operator<<(std::ostream& os,
                         const EnrollmentStatus::Code& enrollment_code);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_ENROLLMENT_ENROLLMENT_STATUS_H_
