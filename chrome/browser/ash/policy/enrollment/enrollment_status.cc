// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"

#include <string_view>

namespace policy {
namespace {
std::string_view ToStringView(EnrollmentStatus::Code enrollment_code) {
  switch (enrollment_code) {
    case EnrollmentStatus::Code::kSuccess:
      return "Success";
    case EnrollmentStatus::Code::kNoStateKeys:
      return "NoStateKeys";
    case EnrollmentStatus::Code::kRegistrationFailed:
      return "RegistrationFailed";
    case EnrollmentStatus::Code::kRegistrationBadMode:
      return "RegistrationBadMode";
    case EnrollmentStatus::Code::kRobotAuthFetchFailed:
      return "RobotAuthFetchFailed";
    case EnrollmentStatus::Code::kRobotRefreshFetchFailed:
      return "RobotRefreshFetchFailed";
    case EnrollmentStatus::Code::kRobotRefreshStoreFailed:
      return "RobotRefreshStoreFailed";
    case EnrollmentStatus::Code::kPolicyFetchFailed:
      return "PolicyFetchFailed";
    case EnrollmentStatus::Code::kValidationFailed:
      return "ValidationFailed";
    case EnrollmentStatus::Code::kLockError:
      return "LockError";
    case EnrollmentStatus::Code::kStoreError:
      return "StoreError";
    case EnrollmentStatus::Code::kAttributeUpdateFailed:
      return "AttributeUpdateFailed";
    case EnrollmentStatus::Code::kRegistrationCertFetchFailed:
      return "RegistrationCertFetchFailed";
    case EnrollmentStatus::Code::kNoMachineIdentification:
      return "NoMachineIdentification";
    case EnrollmentStatus::Code::kDmTokenStoreFailed:
      return "DmTokenStoreFailed";
    case EnrollmentStatus::Code::kMayNotBlockDevMode:
      return "MayNotBlockDevMode";
  }
}
}  // namespace

// static
EnrollmentStatus EnrollmentStatus::ForEnrollmentCode(Code enrollment_code) {
  EnrollmentStatus status;
  status.enrollment_code_ = enrollment_code;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForAttestationError(
    ash::attestation::AttestationStatus attestation_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kRegistrationCertFetchFailed;
  status.attestation_status_ = attestation_status;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForRegistrationError(
    DeviceManagementStatus client_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kRegistrationFailed;
  status.client_status_ = client_status;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForRobotAuthFetchError(
    DeviceManagementStatus client_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kRobotAuthFetchFailed;
  status.client_status_ = client_status;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForFetchError(
    DeviceManagementStatus client_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kPolicyFetchFailed;
  status.client_status_ = client_status;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForValidationError(
    CloudPolicyValidatorBase::Status validation_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kValidationFailed;
  status.validation_status_ = validation_status;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForStoreError(
    CloudPolicyStore::Status store_error,
    CloudPolicyValidatorBase::Status validation_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kStoreError;
  status.store_status_ = store_error;
  return status;
}

// static
EnrollmentStatus EnrollmentStatus::ForLockError(
    ash::InstallAttributes::LockResult lock_status) {
  EnrollmentStatus status;
  status.enrollment_code_ = Code::kLockError;
  status.lock_status_ = lock_status;
  return status;
}

EnrollmentStatus::EnrollmentStatus() = default;

std::ostream& operator<<(std::ostream& os,
                         const EnrollmentStatus::Code& enrollment_code) {
  return os << ToStringView(enrollment_code);
}

}  // namespace policy
