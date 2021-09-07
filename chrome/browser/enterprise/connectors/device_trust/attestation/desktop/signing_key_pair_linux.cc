// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/signing_key_pair.h"

#include "base/notreached.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

namespace {
class SigningKeyPairLinux : public SigningKeyPair {
 public:
  // SigningKeyPair:
  std::unique_ptr<crypto::UnexportableKeyProvider> GetTpmBackedKeyProvider()
      override;
  bool StoreKeyPair(KeyTrustLevel trust_level,
                    std::vector<uint8_t> wrapped) override;
  KeyInfo LoadKeyPair() override;
};

std::unique_ptr<crypto::UnexportableKeyProvider>
SigningKeyPairLinux::GetTpmBackedKeyProvider() {
  return crypto::GetUnexportableKeyProvider();
}

bool SigningKeyPairLinux::StoreKeyPair(KeyTrustLevel trust_level,
                                       std::vector<uint8_t> wrapped) {
  NOTIMPLEMENTED();
  return false;
}

SigningKeyPair::KeyInfo SigningKeyPairLinux::LoadKeyPair() {
  NOTIMPLEMENTED();
  return {BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED, std::vector<uint8_t>()};
}

}  // namespace

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::CreatePlatformKeyPair() {
  // TODO(b/194891140): Take care of the NOTIMPLEMENTED above.
  return std::make_unique<SigningKeyPairLinux>();
}

}  // namespace enterprise_connectors
