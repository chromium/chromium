// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"

namespace enterprise_connectors {

// Service in charge of retrieving context-aware signals for its consumers.
class SignalsService {
 public:
  virtual ~SignalsService() = default;

  // Collects the signals based on the current environment and returns them.
  virtual DeviceTrustSignals CollectSignals() = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_SIGNALS_SIGNALS_SERVICE_H_
