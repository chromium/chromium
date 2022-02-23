// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"

namespace enterprise_connectors {

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::Create() {
  return std::make_unique<KeyRotationManagerImpl>(
      CreateKeyNetworkDelegate(),
      KeyPersistenceDelegateFactory::GetInstance()
          ->CreateKeyPersistenceDelegate(),
      /*sleep_during_backoff=*/true);
}

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::CreateForTesting(
    std::unique_ptr<KeyNetworkDelegate> network_delegate,
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate) {
  return std::make_unique<KeyRotationManagerImpl>(
      std::move(network_delegate), std::move(persistence_delegate),
      /*sleep_during_backoff=*/false);
}

}  // namespace enterprise_connectors
