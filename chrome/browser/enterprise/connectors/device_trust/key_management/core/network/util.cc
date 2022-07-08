// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/util.h"

namespace enterprise_connectors {

class KeyNetworkDelegate;

UploadKeyStatus ParseUploadKeyStatus(
    KeyNetworkDelegate::HttpResponseCode response_code) {
  int status_leading_digit = response_code / 100;
  if (status_leading_digit == 2)
    return UploadKeyStatus::kSucceeded;

  if (status_leading_digit == 4)
    return UploadKeyStatus::kFailed;

  return UploadKeyStatus::kFailedRetryable;
}

}  // namespace enterprise_connectors
