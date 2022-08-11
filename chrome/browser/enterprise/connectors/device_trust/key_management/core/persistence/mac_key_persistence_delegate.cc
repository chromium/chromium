// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/mac_key_persistence_delegate.h"

#include "base/notreached.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_pair.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace enterprise_connectors {

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

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

std::unique_ptr<SigningKeyPair> MacKeyPersistenceDelegate::LoadKeyPair() {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SigningKeyPair> MacKeyPersistenceDelegate::CreateKeyPair() {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace enterprise_connectors
