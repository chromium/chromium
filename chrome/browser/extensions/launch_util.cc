// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/launch_util.h"

#include <optional>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/extension_sync_service.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/launch_util.h"
#include "extensions/common/extension.h"

namespace extensions {

void SetLaunchType(content::BrowserContext* context,
                   const std::string& extension_id,
                   LaunchType launch_type) {
  SetLaunchTypePrefValue(context, extension_id, launch_type);

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
#if BUILDFLAG(IS_CHROMEOS)
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
    NOTREACHED() << static_cast<int>(manifest_launch_container);
  }

  // All paths should set |result|.
  if (!result) {
    DLOG(FATAL) << "Failed to set a launch container.";
    result = apps::LaunchContainer::kLaunchContainerTab;
  }

  return *result;
}

}  // namespace extensions
