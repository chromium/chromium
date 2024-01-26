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
  if (levels.find(DTCPolicyLevel::kBrowser) == levels.end()) {
    std::move(done_closure).Run();
    return;
  }

  auto dm_token = dm_token_storage_->RetrieveDMToken();
  if (dm_token.is_valid()) {
    key_info.set_dm_token(dm_token.value());
  }

  // The device_id is necessary to validate the dm_token.
  key_info.set_device_id(dm_token_storage_->RetrieveClientId());

  if (browser_cloud_policy_store_ &&
      browser_cloud_policy_store_->has_policy()) {
    const auto* policy = browser_cloud_policy_store_->policy();
    key_info.set_customer_id(policy->obfuscated_customer_id());
  }

  key_manager_->ExportPublicKeyAsync(base::BindOnce(
      &DeviceAttester::OnPublicKeyExported, weak_factory_.GetWeakPtr(),
      std::ref(key_info), std::move(done_closure)));
}

void DeviceAttester::SignResponse(const std::set<DTCPolicyLevel>& levels,
                                  const std::string& challenge_response,
                                  SignedData& signed_data,
                                  base::OnceClosure done_closure) {
  if (levels.find(DTCPolicyLevel::kBrowser) == levels.end()) {
    std::move(done_closure).Run();
    return;
  }

  key_manager_->SignStringAsync(
      challenge_response,
      base::BindOnce(&DeviceAttester::OnResponseSigned,
                     weak_factory_.GetWeakPtr(), std::ref(signed_data),
                     std::move(done_closure)));
}

void DeviceAttester::OnPublicKeyExported(
    KeyInfo& key_info,
    base::OnceClosure done_closure,
    std::optional<std::string> exported_key) {
  if (exported_key) {
    key_info.set_browser_instance_public_key(exported_key.value());
  }

  std::move(done_closure).Run();
}

void DeviceAttester::OnResponseSigned(
    SignedData& signed_data,
    base::OnceClosure done_closure,
    std::optional<std::vector<uint8_t>> signed_response) {
  if (signed_response.has_value()) {
    signed_data.set_signature(signed_response->data(), signed_response->size());
  }

  std::move(done_closure).Run();
}

}  // namespace enterprise_connectors
