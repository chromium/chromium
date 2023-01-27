// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/force_installed_tracker_lacros.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"

ForceInstalledTrackerLacros::ForceInstalledTrackerLacros() = default;

ForceInstalledTrackerLacros::~ForceInstalledTrackerLacros() = default;

void ForceInstalledTrackerLacros::Start() {
  if (!IsServiceAvailable()) {
    LOG(ERROR) << "Force installed tracker is not available.";
    return;
  }

  extensions::ForceInstalledTracker* tracker =
      GetExtensionForceInstalledTracker();
  if (!tracker) {
    LOG(ERROR) << "extensions::ForceInstalledTracker is not available.";
    return;
  }

  if (tracker->IsReady())
    OnForceInstalledExtensionsReady();
  else
    observation_.Observe(tracker);
}

void ForceInstalledTrackerLacros::OnForceInstalledExtensionsReady() {
  // Remove the observer if any has been registered.
  observation_.Reset();

  // Notify Ash-chrome via mojom.
  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::ForceInstalledTracker>()
      ->OnForceInstalledExtensionsReady();
}

bool ForceInstalledTrackerLacros::IsServiceAvailable() const {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  CHECK(service);
  return service->IsAvailable<crosapi::mojom::ForceInstalledTracker>();
}

extensions::ForceInstalledTracker*
ForceInstalledTrackerLacros::GetExtensionForceInstalledTracker() {
  extensions::ExtensionSystem* system =
      extensions::ExtensionSystem::Get(ProfileManager::GetPrimaryUserProfile());
  CHECK(system);
  extensions::ExtensionService* service = system->extension_service();
  CHECK(service);
  extensions::ForceInstalledTracker* tracker =
      service->force_installed_tracker();
  return tracker;
}
