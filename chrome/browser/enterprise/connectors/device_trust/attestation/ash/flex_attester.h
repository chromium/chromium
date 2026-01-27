// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_FLEX_ATTESTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_FLEX_ATTESTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attester.h"

class Profile;

namespace enterprise_connectors {

// This class is in charge of populating device signals for ChromeOS Flex
// devices. It is a no-op attester that does not sign the challenge response.
class FlexAttester : public Attester {
 public:
  explicit FlexAttester(Profile* profile);
  ~FlexAttester() override;

  // Attester:
  void DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                       KeyInfo& key_info,
                       base::OnceClosure done_closure) override;
  void SignResponse(const std::set<DTCPolicyLevel>& levels,
                    const std::string& challenge_response,
                    SignedData& signed_data,
                    base::OnceClosure done_closure) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_ASH_FLEX_ATTESTER_H_
