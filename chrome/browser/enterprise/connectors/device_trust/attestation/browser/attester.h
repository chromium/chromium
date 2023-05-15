// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_ATTESTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_ATTESTER_H_

#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/proto/device_trust_attestation_ca.pb.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"

namespace enterprise_connectors {

// Interface for classes to carry out specific actions of building a challenge
// response based on different policy levels.
class Attester {
 public:
  virtual ~Attester() = default;

  // Decorates the `key_info` object based on the policy `levels`.
  // `done_closure` is invoked to indicate the completion of this action.
  virtual void DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                               KeyInfo& key_info,
                               base::OnceClosure done_closure) = 0;

  // Based on the policy `levels`, this signs the given `challenge_response` and
  // populates the `signed_data` with the produced signature. `done_closure` is
  // invoked to indicate the completion this action.
  virtual void SignResponse(const std::set<DTCPolicyLevel>& levels,
                            const std::string& challenge_response,
                            SignedData& signed_data,
                            base::OnceClosure done_closure) = 0;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_ATTESTER_H_
