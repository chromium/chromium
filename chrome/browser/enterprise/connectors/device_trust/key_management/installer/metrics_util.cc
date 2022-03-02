// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/metrics_util.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace enterprise_connectors {

void RecordRotationStatus(const std::string& nonce, RotationStatus status) {
  if (nonce.empty()) {
    base::UmaHistogramEnumeration(
        "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.Status", status);
  } else {
    base::UmaHistogramEnumeration(
        "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.Status", status);
  }
}

void RecordUploadCode(const std::string& nonce, int status_code) {
  if (nonce.empty()) {
    base::UmaHistogramSparse(
        "Enterprise.DeviceTrust.RotateSigningKey.NoNonce.UploadCode",
        status_code);
  } else {
    base::UmaHistogramSparse(
        "Enterprise.DeviceTrust.RotateSigningKey.WithNonce.UploadCode",
        status_code);
  }
}

}  // namespace enterprise_connectors
