// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"

namespace enterprise_connectors {

struct AttestationResponse;

// Interface for classes in charge of building challenge-responses to enable
// handshake between Chrome, an IdP and Verified Access.
class AttestationService {
 public:
  using AttestationCallback =
      base::OnceCallback<void(const AttestationResponse&)>;

  virtual ~AttestationService() = default;

  // Uses the `challenge` that comes from Verified Access to build a proper
  // response including the `signals` and signature representing the device
  // identity, and returns it via the given `callback`. If the challenge does
  // not come from VA, runs `callback` with an empty string.
  // `challenge` represents a serialized SignedData proto.
  virtual void BuildChallengeResponseForVAChallenge(
      const std::string& challenge,
      base::Value::Dict signals,
      AttestationCallback callback) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_COMMON_ATTESTATION_SERVICE_H_
