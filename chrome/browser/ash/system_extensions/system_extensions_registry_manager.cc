// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_registry_manager.h"

#include "chrome/browser/ash/system_extensions/system_extension.h"

namespace ash {

SystemExtensionsRegistryManager::SystemExtensionsRegistryManager() = default;

SystemExtensionsRegistryManager::~SystemExtensionsRegistryManager() = default;

void SystemExtensionsRegistryManager::AddSystemExtension(
    SystemExtension system_extension) {
  registry_.AddSystemExtension(std::move(system_extension));
}

void SystemExtensionsRegistryManager::RemoveSystemExtension(
    const SystemExtensionId& system_extension_id) {
  registry_.RemoveSystemExtension(system_extension_id);
}

}  // namespace ash
