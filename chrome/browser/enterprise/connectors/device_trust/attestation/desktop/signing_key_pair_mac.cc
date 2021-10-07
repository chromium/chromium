// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include "base/notreached.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

bool SigningKeyPair::PersistenceDelegate::StoreKeyPair(
    KeyTrustLevel trust_level,
    std::vector<uint8_t> wrapped) {
  NOTIMPLEMENTED();
  return false;
}

SigningKeyPair::KeyInfo SigningKeyPair::PersistenceDelegate::LoadKeyPair() {
  NOTIMPLEMENTED();
  return {BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()};
}

std::string SigningKeyPair::NetworkDelegate::SendPublicKeyToDmServerSync(
    const std::string& url,
    const std::string& dm_token,
    const std::string& body) {
  NOTIMPLEMENTED();
  return std::string();
}

std::unique_ptr<crypto::UnexportableKeyProvider>
SigningKeyPair::GetTpmBackedKeyProvider() {
  return nullptr;
}

}  // namespace enterprise_connectors
