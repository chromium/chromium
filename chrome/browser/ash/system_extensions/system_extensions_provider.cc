// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_extensions/system_extensions_provider.h"

#include "chrome/browser/ash/system_extensions/system_extension.h"
#include "chrome/browser/ash/system_extensions/system_extensions_install_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_persistent_storage.h"
#include "chrome/browser/ash/system_extensions/system_extensions_profile_utils.h"
#include "chrome/browser/ash/system_extensions/system_extensions_provider_factory.h"
#include "chrome/browser/ash/system_extensions/system_extensions_registry_manager.h"
#include "chrome/browser/ash/system_extensions/system_extensions_service_worker_manager.h"
#include "content/public/common/url_constants.h"

namespace ash {

// TODO:(https://crbug.com/1192426): Change this to system extension scheme when
// it's ready.
const char* kSystemExtensionScheme = content::kChromeUIUntrustedScheme;

// static
SystemExtensionsProvider& SystemExtensionsProvider::Get(Profile* profile) {
  DCHECK(ash::IsSystemExtensionsEnabled(profile));
  return *SystemExtensionsProviderFactory::GetForProfileIfExists(profile);
}

SystemExtensionsProvider::SystemExtensionsProvider(Profile* profile) {
  persistent_storage_ =
      std::make_unique<SystemExtensionsPersistentStorage>(profile);
  registry_manager_ = std::make_unique<SystemExtensionsRegistryManager>();
  service_worker_manager_ =
      std::make_unique<SystemExtensionsServiceWorkerManager>(
          profile, registry_manager_->registry());
  install_manager_ = std::make_unique<SystemExtensionsInstallManager>(
      profile, *registry_manager_, registry_manager_->registry(),
      *service_worker_manager_, *persistent_storage_);
}

SystemExtensionsProvider::~SystemExtensionsProvider() = default;

void SystemExtensionsProvider::
    UpdateEnabledBlinkRuntimeFeaturesInIsolatedWorker(
        const GURL& script_url,
        std::vector<std::string>& out_forced_enabled_runtime_features) {
  if (!script_url.SchemeIs(kSystemExtensionScheme))
    return;

  auto* system_extension = registry().GetByUrl(script_url);
  if (!system_extension)
    return;

  // TODO(https://crbug.com/1272371): Change the following to query system
  // extension feature list.
  out_forced_enabled_runtime_features.push_back("BlinkExtensionChromeOS");
  if (system_extension->type == SystemExtensionType::kWindowManagement) {
    out_forced_enabled_runtime_features.push_back(
        "BlinkExtensionChromeOSWindowManagement");
  }
  if (system_extension->type ==
      SystemExtensionType::kManagedDeviceHealthServices) {
    out_forced_enabled_runtime_features.push_back(
        "BlinkExtensionChromeOSTelemetry");
  }
}

}  // namespace ash
