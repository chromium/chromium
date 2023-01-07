// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_MANAGER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_MANAGER_H_

#include "chrome/browser/ash/system_extensions/system_extensions_mutable_registry.h"

namespace ash {

struct SystemExtension;

// SystemExtensionsRegistryManager drives the stages of registering and
// unregistering system extensions in memory. It uses
// SystemExtensionsMutableRegistry to store the SystemExtension instances. Other
// classes may query the registry directly, but only
// SystemExtensionsRegistryManager is able to make changes to it.
class SystemExtensionsRegistryManager {
 public:
  SystemExtensionsRegistryManager();
  SystemExtensionsRegistryManager(const SystemExtensionsRegistryManager&) =
      delete;
  SystemExtensionsRegistryManager& operator=(
      const SystemExtensionsRegistryManager&) = delete;
  ~SystemExtensionsRegistryManager();

  // Returns the registry this class uses to store SystemExtension instances.
  SystemExtensionsRegistry& registry() { return registry_; }

  // Adds the `system_extension` to the SystemExtensionRegistry.
  void AddSystemExtension(SystemExtension system_extension);

  // Removes the System Extension with `system_extension_id` from the registry
  // if it exists.
  void RemoveSystemExtension(const SystemExtensionId& system_extension_id);

 private:
  SystemExtensionsMutableRegistry registry_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_REGISTRY_MANAGER_H_
