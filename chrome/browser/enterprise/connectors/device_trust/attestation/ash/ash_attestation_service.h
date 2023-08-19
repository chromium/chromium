// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_service.h"

namespace enterprise_connectors {

// This class is in charge of handling the key pair used for attestation. Also
// provides the methods needed in the handshake between Chrome, an IdP and
// Verified Access.
class AshAttestationService : public AttestationService {
 public:
  ~AshAttestationService() override = default;

  // Tries to generate a key-certificate combination for the current profile in
  // case the service will use the flow type DEVICE_TRUST_CONNECTOR for
  // attestation. A failed key preparation will lead to an increased latency
  // during the first DTC flow.
  virtual void TryPrepareKey() = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_ASH_ATTESTATION_SERVICE_H_
