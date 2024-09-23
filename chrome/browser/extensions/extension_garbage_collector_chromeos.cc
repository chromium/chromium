// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/functional/bind.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_assets_manager_chromeos.h"
#include "chrome/browser/extensions/extension_garbage_collector_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/extension_file_task_runner.h"

namespace extensions {

bool ExtensionGarbageCollectorChromeOS::shared_extensions_garbage_collected_ =
    false;

ExtensionGarbageCollectorChromeOS::ExtensionGarbageCollectorChromeOS(
    content::BrowserContext* context)
    : ExtensionGarbageCollector(context),
      disable_garbage_collection_(false) {
}

ExtensionGarbageCollectorChromeOS::~ExtensionGarbageCollectorChromeOS() {}

// static
ExtensionGarbageCollectorChromeOS* ExtensionGarbageCollectorChromeOS::Get(
    content::BrowserContext* context) {
  return static_cast<ExtensionGarbageCollectorChromeOS*>(
      ExtensionGarbageCollector::Get(context));
}

// static
void ExtensionGarbageCollectorChromeOS::ClearGarbageCollectedForTesting() {
  shared_extensions_garbage_collected_ = false;
}

void ExtensionGarbageCollectorChromeOS::GarbageCollectExtensions() {
  if (disable_garbage_collection_)
    return;

  // Process per-profile extensions dir.
  ExtensionGarbageCollector::GarbageCollectExtensions();

  if (!shared_extensions_garbage_collected_ &&
      CanGarbageCollectSharedExtensions()) {
    GarbageCollectSharedExtensions();
    shared_extensions_garbage_collected_ = true;
  }
}

bool ExtensionGarbageCollectorChromeOS::CanGarbageCollectSharedExtensions() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (!user_manager) {
    NOTREACHED_IN_MIGRATION();
    return false;
  }

  const user_manager::UserList& active_users = user_manager->GetLoggedInUsers();
  for (size_t i = 0; i < active_users.size(); i++) {
    // If the profile for one of the active users is still initializing, we
    // can't garbage collect.
    if (!active_users[i]->is_profile_created())
      return false;
    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByUser(active_users[i]);
    ExtensionGarbageCollectorChromeOS* gc =
        ExtensionGarbageCollectorChromeOS::Get(profile);
    if (gc && gc->crx_installs_in_progress_ > 0)
      return false;
  }

  return true;
}

void ExtensionGarbageCollectorChromeOS::GarbageCollectSharedExtensions() {
  std::multimap<std::string, base::FilePath> paths;
  if (ExtensionAssetsManagerChromeOS::CleanUpSharedExtensions(&paths)) {
    if (!GetExtensionFileTaskRunner()->PostTask(
            FROM_HERE,
            base::BindOnce(
                &GarbageCollectExtensionsOnFileThread,
                ExtensionAssetsManagerChromeOS::GetSharedInstallDir(), paths,
                // No need to process unpacked because shared extensions can't
                // be unpacked.
                /*unpacked=*/false))) {
      NOTREACHED_IN_MIGRATION();
    }
  }
}

}  // namespace extensions
