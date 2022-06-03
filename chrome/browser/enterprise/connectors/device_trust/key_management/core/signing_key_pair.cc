// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

// static
absl::optional<SigningKeyPair> SigningKeyPair::Create(
    KeyPersistenceDelegate* persistence_delegate) {
  DCHECK(persistence_delegate);

  KeyTrustLevel trust_level = BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED;
  std::vector<uint8_t> wrapped;
  std::tie(trust_level, wrapped) = persistence_delegate->LoadKeyPair();

  if (wrapped.empty()) {
    // No persisted key pair with a known trust level found.  This is not an
    // error, it could be that no key has been created yet.
    return absl::nullopt;
  }

  std::unique_ptr<crypto::UnexportableSigningKey> key_pair;
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_TPM_KEY: {
      auto provider = persistence_delegate->GetTpmBackedKeyProvider();
      if (provider) {
        key_pair = provider->FromWrappedSigningKeySlowly(wrapped);
      }
      break;
    }
    case BPKUR::CHROME_BROWSER_OS_KEY: {
      ECSigningKeyProvider provider;
      key_pair = provider.FromWrappedSigningKeySlowly(wrapped);
      break;
    }
    case BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED:
      NOTREACHED();
      return absl::nullopt;
  }

  if (key_pair) {
    return SigningKeyPair(std::move(key_pair), trust_level);
  }
  return absl::nullopt;
}

SigningKeyPair::SigningKeyPair(
    std::unique_ptr<crypto::UnexportableSigningKey> key_pair,
    KeyTrustLevel trust_level)
    : key_pair_(std::move(key_pair)), trust_level_(trust_level) {
  DCHECK(key_pair_);
}

SigningKeyPair::SigningKeyPair(SigningKeyPair&& other) = default;
SigningKeyPair& SigningKeyPair::operator=(SigningKeyPair&& other) = default;

SigningKeyPair::~SigningKeyPair() = default;

}  // namespace enterprise_connectors
