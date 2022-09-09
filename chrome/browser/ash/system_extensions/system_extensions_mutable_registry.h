// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_MUTABLE_REGISTRY_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_MUTABLE_REGISTRY_H_

#include <map>

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_registry.h"

namespace ash {

struct SystemExtension;

// SystemExtensionsMutableRegistry implements SystemExtensionsRegistry but
// exposes methods to modify the registry. It's owned by and should only be
// used by SystemExtensionsRegistryManager.
class SystemExtensionsMutableRegistry : public SystemExtensionsRegistry {
 public:
  SystemExtensionsMutableRegistry();
  SystemExtensionsMutableRegistry(const SystemExtensionsMutableRegistry&) =
      delete;
  SystemExtensionsMutableRegistry& operator=(
      const SystemExtensionsMutableRegistry&) = delete;
  ~SystemExtensionsMutableRegistry();

  // Adds |system_extension| to the map of installed System Extensions.
  void AddSystemExtension(SystemExtension system_extension);

  // Removes the System Extension with `system_extension_id` from the
  // map of installed System Extensions.
  void RemoveSystemExtension(const SystemExtensionId& system_extension_id);

  // SystemExtensionsRegistry
  std::vector<SystemExtensionId> GetIds() override;
  const SystemExtension* GetById(
      const SystemExtensionId& system_extension_id) override;
  const SystemExtension* GetByUrl(const GURL& url) override;

 private:
  std::map<SystemExtensionId, SystemExtension> system_extensions_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_MUTABLE_REGISTRY_H_
