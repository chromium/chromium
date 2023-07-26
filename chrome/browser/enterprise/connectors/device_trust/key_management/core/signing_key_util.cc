// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/signing_key_util.h"

#include "base/check.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"

namespace enterprise_connectors {

scoped_refptr<SigningKeyPair> LoadPersistedKey() {
  auto* factory = KeyPersistenceDelegateFactory::GetInstance();
  DCHECK(factory);
  return factory->CreateKeyPersistenceDelegate()->LoadKeyPair();
}

}  // namespace enterprise_connectors
