// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/memory_signing_key_pair.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {
namespace test {

namespace {

SigningKeyPair::KeyTrustLevel g_trust_level =
    BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;

std::vector<uint8_t>* GetWrappedKeyStorage() {
  static base::NoDestructor<std::vector<uint8_t>> wrapped;
  return wrapped.get();
}

}  // namespace

bool InMemorySigningKeyPairPersistenceDelegate::StoreKeyPair(
    SigningKeyPair::KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  if (force_store_to_fail_)
    return false;

  g_trust_level = trust_level;
  *GetWrappedKeyStorage() = std::move(wrapped);
  return true;
}

SigningKeyPair::KeyInfo
InMemorySigningKeyPairPersistenceDelegate::LoadKeyPair() {
  return {g_trust_level, *GetWrappedKeyStorage()};
}

InMemorySigningKeyPairNetworkDelegate::InMemorySigningKeyPairNetworkDelegate() =
    default;
InMemorySigningKeyPairNetworkDelegate::
    ~InMemorySigningKeyPairNetworkDelegate() = default;

std::string InMemorySigningKeyPairNetworkDelegate::SendPublicKeyToDmServerSync(
    const std::string& url,
    const std::string& dm_token,
    const std::string& body) {
  url_ = url;
  dm_token_ = dm_token;
  body_ = body;

  enterprise_management::DeviceManagementResponse response;
  response.mutable_browser_public_key_upload_response()->set_response_code(
      force_network_to_fail_
          ? enterprise_management::BrowserPublicKeyUploadResponse::
                INVALID_SIGNATURE
          : enterprise_management::BrowserPublicKeyUploadResponse::SUCCESS);
  std::string response_str;
  response.SerializeToString(&response_str);
  return response_str;
}

// static
std::unique_ptr<SigningKeyPair> CreateInMemorySigningKeyPair(
    InMemorySigningKeyPairPersistenceDelegate** pdelegate_ptr,
    InMemorySigningKeyPairNetworkDelegate** ndelegate_ptr) {
  auto pdelegate =
      std::make_unique<InMemorySigningKeyPairPersistenceDelegate>();
  auto ndelegate = std::make_unique<InMemorySigningKeyPairNetworkDelegate>();

  if (pdelegate_ptr)
    *pdelegate_ptr = pdelegate.get();
  if (ndelegate_ptr)
    *ndelegate_ptr = ndelegate.get();

  return SigningKeyPair::CreateWithDelegates(std::move(pdelegate),
                                             std::move(ndelegate));
}

ScopedMemorySigningKeyPairPersistence::ScopedMemorySigningKeyPairPersistence() {
  g_trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  GetWrappedKeyStorage()->clear();
}

ScopedMemorySigningKeyPairPersistence::
    ~ScopedMemorySigningKeyPairPersistence() {
  g_trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  GetWrappedKeyStorage()->clear();
}

}  // namespace test
}  // namespace enterprise_connectors
