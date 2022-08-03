// Copyright 2022 The Chromium Authors. All rights reserved.
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

}  // namespace ash
