// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "components/enterprise/client_certificates/core/browser_cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace enterprise_connectors {

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(LoadPersistedKeyResult result)
    : result(result) {}

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(
    scoped_refptr<SigningKeyPair> key_pair)
    : key_pair(key_pair), result(LoadPersistedKeyResult::kSuccess) {
  CHECK(key_pair);
}

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(
    int status_code,
    scoped_refptr<SigningKeyPair> key_pair)
    : status_code(status_code),
      key_pair(key_pair),
      result(LoadPersistedKeyResult::kSuccess) {
  CHECK(key_pair);
}

KeyLoader::DTCLoadKeyResult::~DTCLoadKeyResult() = default;

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(const DTCLoadKeyResult& other) =
    default;
KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(DTCLoadKeyResult&& other) =
    default;

KeyLoader::DTCLoadKeyResult& KeyLoader::DTCLoadKeyResult::operator=(
    const KeyLoader::DTCLoadKeyResult& other) = default;
KeyLoader::DTCLoadKeyResult& KeyLoader::DTCLoadKeyResult::operator=(
    KeyLoader::DTCLoadKeyResult&& other) = default;

// static
std::unique_ptr<KeyLoader> KeyLoader::Create(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!url_loader_factory) {
    return nullptr;
  }

  if (IsDTCKeyUploadedBySharedAPI()) {
    return std::make_unique<KeyLoaderImpl>(
        std::make_unique<
            enterprise_attestation::BrowserCloudManagementDelegate>(
            dm_token_storage,
            enterprise_attestation::DMServerClient::Create(
                device_management_service, std::move(url_loader_factory))));
  }

  // TODO(b/351201459): Remove when DTCRetryUploadingPublicKeyEnabled is fully
  // launched.
  return std::make_unique<KeyLoaderImpl>(
      dm_token_storage, device_management_service,
      std::make_unique<MojoKeyNetworkDelegate>(std::move(url_loader_factory)));
}

}  // namespace enterprise_connectors
