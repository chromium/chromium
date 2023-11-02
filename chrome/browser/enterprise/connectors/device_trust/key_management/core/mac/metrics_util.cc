// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/metrics_util.h"

#include "base/metrics/histogram_functions.h"

namespace enterprise_connectors {

void RecordKeyOperationStatus(SecureEnclaveOperationStatus operation,
                              SecureEnclaveClient::KeyType type) {
  if (type == SecureEnclaveClient::KeyType::kPermanent) {
    base::UmaHistogramEnumeration(
        "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Permanent",
        operation);
  } else {
    base::UmaHistogramEnumeration(
        "Enterprise.DeviceTrust.Mac.SecureEnclaveOperation.Temporary",
        operation);
  }
}

}  // namespace enterprise_connectors
