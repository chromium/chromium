// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"

namespace enterprise_connectors {

void KeyPersistenceDelegate::CleanupTemporaryKeyData() {}

scoped_refptr<SigningKeyPair> KeyPersistenceDelegate::ReturnLoadKeyError(
    LoadPersistedKeyResult result,
    LoadPersistedKeyResult* out_result) {
  if (out_result) {
    *out_result = result;
  }
  return nullptr;
}

}  // namespace enterprise_connectors
