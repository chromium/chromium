// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_util.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"

namespace enterprise_connectors {

LoadedKey::LoadedKey(
    scoped_refptr<enterprise_connectors::SigningKeyPair> key_pair,
    LoadPersistedKeyResult result)
    : key_pair(std::move(key_pair)), result(result) {}

LoadedKey::~LoadedKey() = default;

LoadedKey::LoadedKey(LoadedKey&&) = default;
LoadedKey& LoadedKey::operator=(LoadedKey&&) = default;

LoadedKey LoadPersistedKey() {
  auto* factory = KeyPersistenceDelegateFactory::GetInstance();
  CHECK(factory);

  LoadPersistedKeyResult result;
  auto key_pair = factory->CreateKeyPersistenceDelegate()->LoadKeyPair(
      KeyStorageType::kPermanent, &result);
  return LoadedKey(std::move(key_pair), result);
}

}  // namespace enterprise_connectors
