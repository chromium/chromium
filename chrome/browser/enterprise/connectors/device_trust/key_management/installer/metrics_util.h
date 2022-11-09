// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_

namespace enterprise_connectors {

// Status of rotation attempts made with Rotate().
// Must be kept in sync with the DeviceTrustKeyRotationStatus UMA enum.
enum class RotationStatus {
  SUCCESS,
  FAILURE_CANNOT_GENERATE_NEW_KEY,
  FAILURE_CANNOT_STORE_KEY,
  FAILURE_CANNOT_BUILD_REQUEST,
  FAILURE_CANNOT_UPLOAD_KEY,
  FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED,
  FAILURE_CANNOT_UPLOAD_KEY_RESTORE_FAILED,
  FAILURE_CANNOT_UPLOAD_KEY_TRIES_EXHAUSTED_RESTORE_FAILED,
  FAILURE_INCORRECT_FILE_PERMISSIONS,
  FAILURE_INVALID_ROTATION_PARAMS,
  FAILURE_INVALID_DMSERVER_URL,
  FAILURE_INVALID_DMTOKEN,
  kMaxValue = FAILURE_INVALID_DMTOKEN,
};

// Metrics for the Rotate result. `is_rotation` is used to differentiate a
// create from a rotate attempt and `status` is the result of the action.
void RecordRotationStatus(bool is_rotation, RotationStatus status);

// Metrics for the network delegates upload key result. `is_rotate` is used to
// differentiate a create from a rotate attempt and `status_code` is the HTTP
// response code from the upload key request.
void RecordUploadCode(bool is_rotation, int status_code);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_
