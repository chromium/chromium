// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_

#include "base/callback.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_interface.pb.h"
#include "components/enterprise/common/proto/device_trust_report_event.pb.h"

namespace enterprise_connectors {

// Interface for classes in charge of building challenge-responses to enable
// handshake between Chrome, an IdP and Verified Access.
class AttestationService {
 public:
  using AttestationCallback = base::OnceCallback<void(const std::string&)>;

  virtual ~AttestationService();

  // If the |challenge| comes from Verified Access, invoke |callback| with the
  // proper challenge response, otherwise reply with empty string.
  virtual void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      AttestationCallback callback) = 0;

  // Applies, if any, updates to a |report| about to be sent.
  virtual void StampReport(DeviceTrustReportEvent& report);
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_
