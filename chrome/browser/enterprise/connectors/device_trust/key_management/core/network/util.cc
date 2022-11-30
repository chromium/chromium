// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"

namespace enterprise_connectors {

namespace {

// This error occurs when a public key already exists on the server for the
// current device, and the key in the upload request is not signed by the key
// that already exists.
constexpr KeyNetworkDelegate::HttpResponseCode kKeyConflictCode = 409;

}  // namespace

class KeyNetworkDelegate;

UploadKeyStatus ParseUploadKeyStatus(
    KeyNetworkDelegate::HttpResponseCode response_code) {
  int status_leading_digit = response_code / 100;
  if (status_leading_digit == 2)
    return UploadKeyStatus::kSucceeded;

  if (status_leading_digit == 4) {
    return response_code == kKeyConflictCode
               ? UploadKeyStatus::kFailedKeyConflict
               : UploadKeyStatus::kFailed;
  }

  return UploadKeyStatus::kFailedRetryable;
}

}  // namespace enterprise_connectors
