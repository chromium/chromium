// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/shortcut_manager.h"

#include <string>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/one_shot_event.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_set.h"

using extensions::Extension;

namespace {

void CreateShortcutsForApp(Profile* profile, const Extension* app) {
  web_app::ShortcutLocations creation_locations;

  // Creates a shortcut for an app in the Chrome Apps subdir of the
  // applications menu, if there is not already one present.
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_AUTOMATED,
                           creation_locations, profile, app, base::DoNothing());
}

// Used to disable shortcut syscalls to prevent tests from flaking.
bool g_suppress_shortcuts_for_testing = false;

}  // namespace

// static
void AppShortcutManager::SuppressShortcutsForTesting() {
  g_suppress_shortcuts_for_testing = true;
}

AppShortcutManager::AppShortcutManager(Profile* profile) : profile_(profile) {
  // Use of g_browser_process requires that we are either on the UI thread, or
  // there are no threads initialized (such as in unit tests).
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extension_registry_observation_.Observe(
      extensions::ExtensionRegistry::Get(profile_));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager might be NULL in testing environments.
  if (profile_manager) {
    profile_storage_observation_.Observe(
        &profile_manager->GetProfileAttributesStorage());
  }
}

AppShortcutManager::~AppShortcutManager() = default;

void AppShortcutManager::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (!extension->is_app() || g_suppress_shortcuts_for_testing) {
    return;
  }

  // If the app is being updated, update any existing shortcuts but do not
  // create new ones. If it is being installed, automatically create a
  // shortcut in the applications menu (e.g., Start Menu).
  if (is_update) {
    web_app::UpdateAllShortcuts(base::UTF8ToUTF16(old_name), profile_,
                                extension, base::DoNothing());
  } else {
    CreateShortcutsForApp(profile_, extension);
  }
}

void AppShortcutManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  if (!g_suppress_shortcuts_for_testing)
    web_app::DeleteAllShortcuts(profile_, extension);
}

void AppShortcutManager::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath() || g_suppress_shortcuts_for_testing) {
    return;
  }

  web_app::internals::GetShortcutIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&web_app::internals::DeleteAllShortcutsForProfile,
                     profile_path));
}
