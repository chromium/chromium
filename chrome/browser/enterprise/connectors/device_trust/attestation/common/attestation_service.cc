// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"

namespace enterprise_connectors {

AttestationService::~AttestationService() = default;

void AttestationService::StampReport(DeviceTrustReportEvent& report) {
  // No-op by default.
}

}  // namespace enterprise_connectors
