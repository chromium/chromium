// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_

#include <memory>

#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_registry_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace ash {

class SystemExtensionsPersistentStorage;
class SystemExtensionsServiceWorkerManager;

// Manages the installation, storage, and execution of System Extensions.
class SystemExtensionsProvider : public KeyedService {
 public:
  // Returns the provider associated with `profile`. Should only be called if
  // System Extensions is enabled for the profile i.e. if
  // IsSystemExtensionsEnabled() returns true.
  static SystemExtensionsProvider& Get(Profile* profile);

  explicit SystemExtensionsProvider(Profile* profile);
  SystemExtensionsProvider(const SystemExtensionsProvider&) = delete;
  SystemExtensionsProvider& operator=(const SystemExtensionsProvider&) = delete;
  ~SystemExtensionsProvider() override;

  SystemExtensionsRegistry& registry() { return registry_manager_->registry(); }

  SystemExtensionsRegistryManager& registry_manager() {
    return *registry_manager_;
  }

  SystemExtensionsServiceWorkerManager& service_worker_manager() {
    return *service_worker_manager_;
  }

  SystemExtensionsPersistentStorage& persistent_storage() {
    return *persistent_storage_;
  }

  SystemExtensionsInstallManager& install_manager() {
    return *install_manager_;
  }

  // Called when a service worker will be started to enable Blink runtime
  // features based on system extension type. Currently System Extensions run on
  // chrome-untrusted:// which is process isolated, so this method should be
  // called.
  void UpdateEnabledBlinkRuntimeFeaturesInIsolatedWorker(
      const GURL& script_url,
      std::vector<std::string>& out_forced_enabled_runtime_features);

 private:
  std::unique_ptr<SystemExtensionsRegistryManager> registry_manager_;
  std::unique_ptr<SystemExtensionsServiceWorkerManager> service_worker_manager_;
  std::unique_ptr<SystemExtensionsPersistentStorage> persistent_storage_;
  std::unique_ptr<SystemExtensionsInstallManager> install_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_H_
