// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"
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
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  if (!url_loader_factory) {
    return nullptr;
  }

  return std::make_unique<KeyLoaderImpl>(
      std::make_unique<enterprise_attestation::BrowserCloudManagementDelegate>(
          enterprise_attestation::DMServerClient::Create(
              device_management_service, std::move(url_loader_factory))));
}

}  // namespace enterprise_connectors
