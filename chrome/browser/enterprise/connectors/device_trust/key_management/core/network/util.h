// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_UTIL_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_UTIL_H_

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"

namespace enterprise_connectors {

// Status of the upload key request.
enum class UploadKeyStatus {
  kSucceeded,
  kFailed,
  kFailedKeyConflict,
  kFailedRetryable,
};

// Returns the status of the upload request based on the `response_code`.
UploadKeyStatus ParseUploadKeyStatus(
    KeyNetworkDelegate::HttpResponseCode response_code);

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_KEY_MANAGEMENT_CORE_NETWORK_UTIL_H_
