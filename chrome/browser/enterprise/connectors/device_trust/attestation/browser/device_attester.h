// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_DEVICE_ATTESTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_DEVICE_ATTESTER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/attester.h"

namespace policy {
class BrowserDMTokenStorage;
class CloudPolicyStore;
}  // namespace policy

namespace enterprise_connectors {

class DeviceTrustKeyManager;

// This class is in charge of populating device specific details during the
// browser attestation process.
class DeviceAttester : public Attester {
 public:
  DeviceAttester(DeviceTrustKeyManager* key_manager,
                 policy::BrowserDMTokenStorage* dm_token_storage,
                 policy::CloudPolicyStore* browser_cloud_policy_store);
  ~DeviceAttester() override;

  // Attester:
  void DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                       KeyInfo& key_info,
                       base::OnceClosure done_closure) override;
  void SignResponse(const std::set<DTCPolicyLevel>& levels,
                    const std::string& challenge_response,
                    SignedData& signed_data,
                    base::OnceClosure done_closure) override;

 private:
  // Adds the `exported_key` details to the `key_info` and invokes the
  // `done_closure` to indicate completion.
  void OnPublicKeyExported(KeyInfo& key_info,
                           base::OnceClosure done_closure,
                           std::optional<std::string> exported_key);

  // Adds the `signed_response` to the `signed_data` and invokes the
  // `done_closure` to indicate completion.
  void OnResponseSigned(SignedData& signed_data,
                        base::OnceClosure done_closure,
                        std::optional<std::vector<uint8_t>> signed_response);

  // Owned by the CBCMController, which is eventually owned by the browser
  // process. Since the current service is owned at the profile level, this
  // respects the browser shutdown sequence.
  const raw_ptr<DeviceTrustKeyManager> key_manager_;

  // Helper for handling the DM token and device ID.
  const raw_ptr<policy::BrowserDMTokenStorage> dm_token_storage_;

  // Used for retrieving a managed devices customer ID.
  const raw_ptr<policy::CloudPolicyStore> browser_cloud_policy_store_;

  base::WeakPtrFactory<DeviceAttester> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_ATTESTATION_BROWSER_DEVICE_ATTESTER_H_
