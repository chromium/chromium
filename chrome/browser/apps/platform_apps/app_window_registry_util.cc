// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_window_registry_util.h"

#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"

using extensions::AppWindow;
using extensions::AppWindowRegistry;

typedef AppWindowRegistry::AppWindowList AppWindowList;
typedef AppWindowRegistry::Factory Factory;

// static
AppWindow* AppWindowRegistryUtil::GetAppWindowForNativeWindowAnyProfile(
    gfx::NativeWindow window) {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator i = profiles.begin();
       i != profiles.end(); ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    AppWindow* app_window = registry->GetAppWindowForNativeWindow(window);
    if (app_window)
      return app_window;
  }

  return nullptr;
}

// static
bool AppWindowRegistryUtil::IsAppWindowVisibleInAnyProfile(
    int window_type_mask) {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator i = profiles.begin();
       i != profiles.end(); ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    const AppWindowList& app_windows = registry->app_windows();
    if (app_windows.empty())
      continue;

    for (const AppWindow* window : app_windows) {
      if (!window->is_hidden() &&
          (window_type_mask == 0 || (window->window_type() & window_type_mask)))
        return true;
    }
  }

  return false;
}

// static
void AppWindowRegistryUtil::CloseAllAppWindows() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (std::vector<Profile*>::const_iterator i = profiles.begin();
       i != profiles.end(); ++i) {
    AppWindowRegistry* registry =
        Factory::GetForBrowserContext(*i, false /* create */);
    if (!registry)
      continue;

    // Ask each app window to close, but cater for windows removing or
    // rearranging themselves in the ordered window list in response.
    AppWindowList window_list_copy(registry->app_windows());
    for (AppWindow* window : window_list_copy) {
      // Ensure window is still valid.
      if (base::Contains(registry->app_windows(), window))
        window->GetBaseWindow()->Close();
    }
  }
}
