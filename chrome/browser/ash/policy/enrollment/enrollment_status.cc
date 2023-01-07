// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_status.h"

#include "build/chromeos_buildflags.h"
#include "net/http/http_status_code.h"

namespace policy {

// static
EnrollmentStatus EnrollmentStatus::ForStatus(Status status) {
  return CreateEnrollmentStatusWithoutLockError(
      status, DM_STATUS_SUCCESS, CloudPolicyStore::STATUS_OK,
      CloudPolicyValidatorBase::VALIDATION_OK);
}

// static
EnrollmentStatus EnrollmentStatus::ForRegistrationError(
    DeviceManagementStatus client_status) {
  return CreateEnrollmentStatusWithoutLockError(
      REGISTRATION_FAILED, client_status, CloudPolicyStore::STATUS_OK,
      CloudPolicyValidatorBase::VALIDATION_OK);
}

// static
EnrollmentStatus EnrollmentStatus::ForRobotAuthFetchError(
    DeviceManagementStatus client_status) {
  return CreateEnrollmentStatusWithoutLockError(
      ROBOT_AUTH_FETCH_FAILED, client_status, CloudPolicyStore::STATUS_OK,
      CloudPolicyValidatorBase::VALIDATION_OK);
}

// static
EnrollmentStatus EnrollmentStatus::ForFetchError(
    DeviceManagementStatus client_status) {
  return CreateEnrollmentStatusWithoutLockError(
      POLICY_FETCH_FAILED, client_status, CloudPolicyStore::STATUS_OK,
      CloudPolicyValidatorBase::VALIDATION_OK);
}

// static
EnrollmentStatus EnrollmentStatus::ForValidationError(
    CloudPolicyValidatorBase::Status validation_status) {
  return CreateEnrollmentStatusWithoutLockError(
      VALIDATION_FAILED, DM_STATUS_SUCCESS, CloudPolicyStore::STATUS_OK,
      validation_status);
}

// static
EnrollmentStatus EnrollmentStatus::ForStoreError(
    CloudPolicyStore::Status store_error,
    CloudPolicyValidatorBase::Status validation_status) {
  return CreateEnrollmentStatusWithoutLockError(STORE_ERROR, DM_STATUS_SUCCESS,
                                                store_error, validation_status);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// static
EnrollmentStatus EnrollmentStatus::ForLockError(
    ash::InstallAttributes::LockResult lock_status) {
  return EnrollmentStatus(LOCK_ERROR, DM_STATUS_SUCCESS,
                          CloudPolicyStore::STATUS_OK,
                          CloudPolicyValidatorBase::VALIDATION_OK, lock_status);
}

EnrollmentStatus::EnrollmentStatus(
    EnrollmentStatus::Status status,
    DeviceManagementStatus client_status,
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status,
    ash::InstallAttributes::LockResult lock_status)
    : status_(status),
      client_status_(client_status),
      store_status_(store_status),
      validation_status_(validation_status),
      lock_status_(lock_status) {}
#else
EnrollmentStatus::EnrollmentStatus(
    EnrollmentStatus::Status status,
    DeviceManagementStatus client_status,
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status)
    : status_(status),
      client_status_(client_status),
      store_status_(store_status),
      validation_status_(validation_status) {}
#endif

// static
EnrollmentStatus EnrollmentStatus::CreateEnrollmentStatusWithoutLockError(
    Status status,
    DeviceManagementStatus client_status,
    CloudPolicyStore::Status store_status,
    CloudPolicyValidatorBase::Status validation_status) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return EnrollmentStatus(status, client_status, store_status,
                          validation_status,
                          ash::InstallAttributes::LOCK_SUCCESS);
#else
  return EnrollmentStatus(status, client_status, store_status,
                          validation_status);
#endif
}

}  // namespace policy
