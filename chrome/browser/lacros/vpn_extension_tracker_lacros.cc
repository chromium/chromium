// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/vpn_extension_tracker_lacros.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/crosapi/mojom/vpn_extension_observer.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/permissions/permissions_data.h"

namespace {

bool IsVpnProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::mojom::APIPermissionID::kVpnProvider);
}

bool IsVpnExtensionObserverAvailable() {
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service->IsAvailable<crosapi::mojom::VpnExtensionObserver>();
}

crosapi::mojom::VpnExtensionObserver* GetVpnExtensionObserver() {
  auto* lacros_service = chromeos::LacrosService::Get();
  return lacros_service->GetRemote<crosapi::mojom::VpnExtensionObserver>()
      .get();
}

}  // namespace

VpnExtensionTrackerLacros::VpnExtensionTrackerLacros() = default;
VpnExtensionTrackerLacros::~VpnExtensionTrackerLacros() = default;

void VpnExtensionTrackerLacros::Start() {
  if (!IsVpnExtensionObserverAvailable()) {
    return;
  }

  auto* profile = ProfileManager::GetPrimaryUserProfile();
  auto* extension_registry = extensions::ExtensionRegistry::Get(profile);
  extension_registry_observer_.Observe(extension_registry);

  // Sync extensions on startup.
  auto* observer = GetVpnExtensionObserver();
  for (const auto& extension : extension_registry->enabled_extensions()) {
    if (!IsVpnProvider(extension.get())) {
      continue;
    }
    observer->OnLacrosVpnExtensionLoaded(extension->id(), extension->name());
  }
}

void VpnExtensionTrackerLacros::OnExtensionLoaded(
    content::BrowserContext*,
    const extensions::Extension* extension) {
  if (!IsVpnProvider(extension)) {
    return;
  }
  GetVpnExtensionObserver()->OnLacrosVpnExtensionLoaded(extension->id(),
                                                        extension->name());
}

void VpnExtensionTrackerLacros::OnExtensionUnloaded(
    content::BrowserContext*,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionReason) {
  if (!IsVpnProvider(extension)) {
    return;
  }
  GetVpnExtensionObserver()->OnLacrosVpnExtensionUnloaded(extension->id());
}

void VpnExtensionTrackerLacros::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  DCHECK(extension_registry_observer_.IsObservingSource(registry));
  extension_registry_observer_.Reset();
}
