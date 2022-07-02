// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"

#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/ec_signing_key.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "crypto/unexportable_key.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace enterprise_connectors {

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::Create(
    KeyPersistenceDelegate* persistence_delegate) {
  DCHECK(persistence_delegate);

  auto [trust_level, wrapped] = persistence_delegate->LoadKeyPair();

  if (wrapped.empty()) {
    // No persisted key pair with a known trust level found.  This is not an
    // error, it could be that no key has been created yet.
    return nullptr;
  }

  std::unique_ptr<crypto::UnexportableSigningKey> key_pair;
  switch (trust_level) {
    case BPKUR::CHROME_BROWSER_HW_KEY: {
      auto provider = persistence_delegate->GetUnexportableKeyProvider();
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
      return nullptr;
  }

  if (key_pair) {
    return std::make_unique<SigningKeyPair>(std::move(key_pair), trust_level);
  }
  return nullptr;
}

// static
std::unique_ptr<SigningKeyPair> SigningKeyPair::LoadPersistedKey() {
  auto* factory = KeyPersistenceDelegateFactory::GetInstance();
  DCHECK(factory);
  auto persistence_delegate = factory->CreateKeyPersistenceDelegate();
  return Create(persistence_delegate.get());
}

SigningKeyPair::SigningKeyPair(
    std::unique_ptr<crypto::UnexportableSigningKey> key_pair,
    KeyTrustLevel trust_level)
    : key_pair_(std::move(key_pair)), trust_level_(trust_level) {
  DCHECK(key_pair_);
}

SigningKeyPair::~SigningKeyPair() = default;

}  // namespace enterprise_connectors
