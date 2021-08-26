// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/desktop/memory_signing_key_pair.h"

#include <stdint.h>

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

MemorySigningKeyPair::MemorySigningKeyPair() = default;

// static
std::unique_ptr<MemorySigningKeyPair> MemorySigningKeyPair::Create() {
  auto key_pair = base::WrapUnique(new MemorySigningKeyPair);
  key_pair->Init();
  return key_pair;
}

bool MemorySigningKeyPair::StoreKeyPair(KeyTrustLevel trust_level,
                                        std::vector<uint8_t> wrapped) {
  if (force_store_to_fail_)
    return false;

  g_trust_level = trust_level;
  *GetWrappedKeyStorage() = std::move(wrapped);
  return true;
}

MemorySigningKeyPair::KeyInfo MemorySigningKeyPair::LoadKeyPair() {
  return {g_trust_level, *GetWrappedKeyStorage()};
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
