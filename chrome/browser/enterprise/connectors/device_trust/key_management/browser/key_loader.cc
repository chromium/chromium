// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader.h"

#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/key_management/browser/key_loader_impl.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/mojo_key_network_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#if BUILDFLAG(IS_WIN)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/win_key_network_delegate.h"
#endif  // BUILDFLAG(IS_WIN)

namespace enterprise_connectors {

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult() = default;
KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(
    int code,
    scoped_refptr<SigningKeyPair> key_pair)
    : status_code(code), key_pair(std::move(key_pair)) {}

KeyLoader::DTCLoadKeyResult::DTCLoadKeyResult(DTCLoadKeyResult&& other) =
    default;

KeyLoader::DTCLoadKeyResult& KeyLoader::DTCLoadKeyResult::operator=(
    KeyLoader::DTCLoadKeyResult&& other) = default;

KeyLoader::DTCLoadKeyResult::~DTCLoadKeyResult() = default;

// static
std::unique_ptr<KeyLoader> KeyLoader::Create(
    policy::BrowserDMTokenStorage* dm_token_storage,
    policy::DeviceManagementService* device_management_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  std::unique_ptr<KeyNetworkDelegate> network_delegate;
  if (url_loader_factory) {
    network_delegate =
        std::make_unique<MojoKeyNetworkDelegate>(std::move(url_loader_factory));
  } else {
#if BUILDFLAG(IS_WIN)
    network_delegate = std::make_unique<WinKeyNetworkDelegate>();
#else
    return nullptr;
#endif  // BUILDFLAG(IS_WIN)
  }

  return std::make_unique<KeyLoaderImpl>(
      dm_token_storage, device_management_service, std::move(network_delegate));
}

}  // namespace enterprise_connectors
