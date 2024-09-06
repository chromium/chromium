// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager.h"

#include <memory>
#include <utility>

#include "chrome/browser/enterprise/connectors/device_trust/device_trust_features.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/network/key_network_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/persistence/key_persistence_delegate_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/key_management/installer/key_rotation_manager_impl.h"
#include "key_rotation_manager.h"

namespace enterprise_connectors {

namespace {

// Used for storing the key rotation manager in tests.
std::unique_ptr<KeyRotationManager>& GetKeyRotationManagerFromStorage() {
  static std::unique_ptr<KeyRotationManager> storage;
  return storage;
}

}  // namespace

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::Create(
    std::unique_ptr<KeyNetworkDelegate> network_delegate) {
  auto& rotation_manager_instance = GetKeyRotationManagerFromStorage();
  if (rotation_manager_instance) {
    return std::move(rotation_manager_instance);
  }

  return std::make_unique<KeyRotationManagerImpl>(
      std::move(network_delegate), KeyPersistenceDelegateFactory::GetInstance()
                                       ->CreateKeyPersistenceDelegate());
}

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::Create() {
  auto& rotation_manager_instance = GetKeyRotationManagerFromStorage();
  if (rotation_manager_instance) {
    return std::move(rotation_manager_instance);
  }

  CHECK(IsDTCKeyRotationUploadedBySharedAPI());

  return std::make_unique<KeyRotationManagerImpl>(
      KeyPersistenceDelegateFactory::GetInstance()
          ->CreateKeyPersistenceDelegate());
}

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::CreateForTesting(
    std::unique_ptr<KeyNetworkDelegate> network_delegate,
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate) {
  return std::make_unique<KeyRotationManagerImpl>(
      std::move(network_delegate), std::move(persistence_delegate));
}

// static
std::unique_ptr<KeyRotationManager> KeyRotationManager::CreateForTesting(
    std::unique_ptr<KeyPersistenceDelegate> persistence_delegate) {
  return std::make_unique<KeyRotationManagerImpl>(
      std::move(persistence_delegate));
}

// static
void KeyRotationManager::SetForTesting(
    std::unique_ptr<KeyRotationManager> key_rotation_manager) {
  auto& storage = GetKeyRotationManagerFromStorage();
  storage = std::move(key_rotation_manager);
}

}  // namespace enterprise_connectors
