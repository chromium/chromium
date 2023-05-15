// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_PROFILE_ATTESTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_PROFILE_ATTESTER_H_

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attester.h"

#include "base/memory/raw_ptr.h"

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace policy {
class CloudPolicyStore;
}  // namespace policy

namespace enterprise_connectors {

// This class is in charge of populating profile specific details during the
// browser attestation process.
class ProfileAttester : public Attester {
 public:
  ProfileAttester(enterprise::ProfileIdService* profile_id_service,
                  policy::CloudPolicyStore* user_cloud_policy_store);
  ~ProfileAttester() override;

  // Attester:
  void DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                       KeyInfo& key_info,
                       base::OnceClosure done_closure) override;
  void SignResponse(const std::set<DTCPolicyLevel>& levels,
                    const std::string& challenge_response,
                    SignedData& signed_data,
                    base::OnceClosure done_closure) override;

 private:
  // Used for retrieving the profile ID.
  const raw_ptr<enterprise::ProfileIdService> profile_id_service_;

  // Used for retrieving a managed profiles customer and gaia ID.
  const raw_ptr<policy::CloudPolicyStore> user_cloud_policy_store_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_PROFILE_ATTESTER_H_
