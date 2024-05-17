// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/launch_util.h"

#include <memory>

#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/bookmark_app_util.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/pref_names.h"
#include "extensions/common/extension.h"

namespace extensions {
namespace {

// A preference set by the the NTP to persist the desired launch container type
// used for apps.
const char kPrefLaunchType[] = "launchType";

}  // namespace

LaunchType GetLaunchType(const ExtensionPrefs* prefs,
                         const Extension* extension) {
  if (!extension) {
    return LAUNCH_TYPE_INVALID;
  }
  LaunchType result = LAUNCH_TYPE_DEFAULT;

  int value = GetLaunchTypePrefValue(prefs, extension->id());
  if (value >= LAUNCH_TYPE_FIRST && value < NUM_LAUNCH_TYPES)
    result = static_cast<LaunchType>(value);

  // Force hosted apps that are not locally installed to open in tabs.
  if (extension->is_hosted_app() &&
      !BookmarkAppIsLocallyInstalled(prefs, extension)) {
    result = LAUNCH_TYPE_REGULAR;
  } else if (result == LAUNCH_TYPE_PINNED) {
    result = LAUNCH_TYPE_REGULAR;
  } else if (result == LAUNCH_TYPE_FULLSCREEN) {
    result = LAUNCH_TYPE_WINDOW;
  }
  return result;
}

LaunchType GetLaunchTypePrefValue(const ExtensionPrefs* prefs,
                                  const std::string& extension_id) {
  int value = LAUNCH_TYPE_INVALID;
  return prefs->ReadPrefAsInteger(extension_id, kPrefLaunchType, &value)
      ? static_cast<LaunchType>(value) : LAUNCH_TYPE_INVALID;
}

void SetLaunchType(content::BrowserContext* context,
                   const std::string& extension_id,
                   LaunchType launch_type) {
  DCHECK(launch_type >= LAUNCH_TYPE_FIRST && launch_type < NUM_LAUNCH_TYPES);

  ExtensionPrefs::Get(context)->UpdateExtensionPref(
      extension_id, kPrefLaunchType,
      base::Value(static_cast<int>(launch_type)));

  // Sync the launch type.
  const Extension* extension =
      ExtensionRegistry::Get(context)
          ->GetExtensionById(extension_id, ExtensionRegistry::EVERYTHING);
  if (extension)
    ExtensionSyncService::Get(context)->SyncExtensionChangeIfNeeded(*extension);
}

apps::LaunchContainer GetLaunchContainer(const ExtensionPrefs* prefs,
                                         const Extension* extension) {
  apps::LaunchContainer manifest_launch_container =
      AppLaunchInfo::GetLaunchContainer(extension);

  std::optional<apps::LaunchContainer> result;

  if (manifest_launch_container ==
      apps::LaunchContainer::kLaunchContainerPanelDeprecated) {
    result = manifest_launch_container;
  } else if (manifest_launch_container ==
             apps::LaunchContainer::kLaunchContainerTab) {
    // Look for prefs that indicate the user's choice of launch container. The
    // app's menu on the NTP provides a UI to set this preference.
    LaunchType prefs_launch_type = GetLaunchType(prefs, extension);

    if (prefs_launch_type == LAUNCH_TYPE_WINDOW) {
      // If the pref is set to launch a window (or no pref is set, and
      // window opening is the default), make the container a window.
      result = apps::LaunchContainer::kLaunchContainerWindow;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    } else if (prefs_launch_type == LAUNCH_TYPE_FULLSCREEN) {
      // LAUNCH_TYPE_FULLSCREEN launches in a maximized app window in ash.
      // For desktop chrome AURA on all platforms we should open the
      // application in full screen mode in the current tab, on the same
      // lines as non AURA chrome.
      result = apps::LaunchContainer::kLaunchContainerWindow;
#endif
    } else {
      // All other launch types (tab, pinned, fullscreen) are
      // implemented as tabs in a window.
      result = apps::LaunchContainer::kLaunchContainerTab;
    }
  } else {
    // If a new value for app.launch.container is added, logic for it should be
    // added here. apps::LaunchContainer::kLaunchContainerWindow is not
    // present because there is no way to set it in a manifest.
    NOTREACHED_IN_MIGRATION() << static_cast<int>(manifest_launch_container);
  }

  // All paths should set |result|.
  if (!result) {
    DLOG(FATAL) << "Failed to set a launch container.";
    result = apps::LaunchContainer::kLaunchContainerTab;
  }

  return *result;
}

bool HasPreferredLaunchContainer(const ExtensionPrefs* prefs,
                                 const Extension* extension) {
  int value = -1;
  apps::LaunchContainer manifest_launch_container =
      AppLaunchInfo::GetLaunchContainer(extension);
  return manifest_launch_container ==
             apps::LaunchContainer::kLaunchContainerTab &&
         prefs->ReadPrefAsInteger(extension->id(), kPrefLaunchType, &value);
}

bool LaunchesInWindow(content::BrowserContext* context,
                      const Extension* extension) {
  return GetLaunchType(ExtensionPrefs::Get(context), extension) ==
         LAUNCH_TYPE_WINDOW;
}

}  // namespace extensions
