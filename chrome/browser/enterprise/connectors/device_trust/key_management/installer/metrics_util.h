// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_

#include <string>

namespace enterprise_connectors {

// Status of rotation attempts made with RotateWithAdminRights().
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
  kMaxValue = FAILURE_INCORRECT_FILE_PERMISSIONS,
};

// Metrics for the RotateWithAdminRights() result. `nonce` is the
// nonce from the rotate attempt and `status` is the status of the
// rotation.
void RecordRotationStatus(const std::string& nonce, RotationStatus status);

// Metrics for the network delegates upload key result. `nonce` is
// the nonce from the rotate attempt and `status_code` is the HTTP
// response code from the upload key request.
void RecordUploadCode(const std::string& nonce, int status_code);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_INSTALLER_METRICS_UTIL_H_
