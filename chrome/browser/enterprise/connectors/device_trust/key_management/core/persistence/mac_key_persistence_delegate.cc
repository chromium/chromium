// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"

#include "base/notreached.h"

namespace enterprise_connectors {

MacKeyPersistenceDelegate::~MacKeyPersistenceDelegate() = default;

bool MacKeyPersistenceDelegate::CheckRotationPermissions() {
  NOTIMPLEMENTED();
  return false;
}

bool MacKeyPersistenceDelegate::StoreKeyPair(KeyTrustLevel trust_level,
                                             std::vector<uint8_t> wrapped) {
  NOTIMPLEMENTED();
  return false;
}

KeyPersistenceDelegate::KeyInfo MacKeyPersistenceDelegate::LoadKeyPair() {
  NOTIMPLEMENTED();
  return invalid_key_info();
}

std::unique_ptr<crypto::UnexportableKeyProvider>
MacKeyPersistenceDelegate::GetTpmBackedKeyProvider() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace enterprise_connectors
