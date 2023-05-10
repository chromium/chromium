// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/browser/device_attester.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/device_trust/device_trust_key_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"

namespace enterprise_connectors {

DeviceAttester::DeviceAttester(
    DeviceTrustKeyManager* key_manager,
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::CloudPolicyStore* browser_cloud_policy_store)
    : key_manager_(key_manager),
      dm_token_storage_(dm_token_storage),
      browser_cloud_policy_store_(browser_cloud_policy_store) {
  CHECK(key_manager_);
  CHECK(dm_token_storage_);
}

DeviceAttester::~DeviceAttester() = default;

void DeviceAttester::DecorateKeyInfo(const std::set<DTCPolicyLevel>& levels,
                                     KeyInfo& key_info,
                                     base::OnceClosure done_closure) {
  // TODO(b:279063303): Populate this with device specific key info.
  std::move(done_closure).Run();
}

void DeviceAttester::SignResponse(const std::set<DTCPolicyLevel>& levels,
                                  const std::string& challenge_response,
                                  SignedData& signed_data,
                                  base::OnceClosure done_closure) {
  // TODO(b:279063303): Invoke the key manager to preform the challenge response
  // signing.
  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
