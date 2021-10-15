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
using BPKUP = enterprise_management::BrowserPublicKeyUploadResponse;

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
    const GURL& url,
    const std::string& dm_token,
    const std::string& body) {
  ++send_count_;
  url_ = url;
  dm_token_ = dm_token;
  body_ = body;

  auto rc = BPKUP::SUCCESS;
  if (!response_codes_.empty()) {
    rc = response_codes_.front();
    response_codes_.pop_front();
  }

  enterprise_management::DeviceManagementResponse response;
  response.mutable_browser_public_key_upload_response()->set_response_code(rc);
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
