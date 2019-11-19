// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/shortcut_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/one_shot_event.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
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

#if defined(OS_MACOSX)
#include "chrome/browser/apps/platform_apps/app_shim_registry_mac.h"
#include "chrome/common/mac/app_mode_common.h"
#endif

using extensions::Extension;

namespace {

// This version number is stored in local prefs to check whether app shortcuts
// need to be recreated. This might happen when we change various aspects of app
// shortcuts like command-line flags or associated icons, binaries, etc.
#if defined(OS_MACOSX)
const int kCurrentAppShortcutsVersion = APP_SHIM_VERSION_NUMBER;
#else
const int kCurrentAppShortcutsVersion = 0;
#endif

// Delay in seconds before running UpdateShortcutsForAllApps.
const int kUpdateShortcutsForAllAppsDelay = 10;

void CreateShortcutsForApp(Profile* profile, const Extension* app) {
  web_app::ShortcutLocations creation_locations;

  // Creates a shortcut for an app in the Chrome Apps subdir of the
  // applications menu, if there is not already one present.
  creation_locations.applications_menu_location =
      web_app::APP_MENU_LOCATION_SUBDIR_CHROMEAPPS;

  web_app::CreateShortcuts(web_app::SHORTCUT_CREATION_AUTOMATED,
                           creation_locations, profile, app, base::DoNothing());
}

}  // namespace

// static
void AppShortcutManager::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Indicates whether app shortcuts have been created.
  registry->RegisterIntegerPref(prefs::kAppShortcutsVersion, 0);
}

AppShortcutManager::AppShortcutManager(Profile* profile)
    : profile_(profile), is_profile_attributes_storage_observer_(false) {
  // Use of g_browser_process requires that we are either on the UI thread, or
  // there are no threads initialized (such as in unit tests).
  DCHECK(!content::BrowserThread::IsThreadInitialized(
             content::BrowserThread::UI) ||
         content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  extension_registry_observer_.Add(
      extensions::ExtensionRegistry::Get(profile_));
  // Wait for extensions to be ready before running
  // UpdateShortcutsForAllAppsIfNeeded.
  extensions::ExtensionSystem::Get(profile)->ready().Post(
      FROM_HERE,
      base::BindOnce(&AppShortcutManager::UpdateShortcutsForAllAppsIfNeeded,
                     weak_ptr_factory_.GetWeakPtr()));

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  // profile_manager might be NULL in testing environments.
  if (profile_manager) {
    profile_manager->GetProfileAttributesStorage().AddObserver(this);
    is_profile_attributes_storage_observer_ = true;
  }
}

AppShortcutManager::~AppShortcutManager() {
  if (g_browser_process && is_profile_attributes_storage_observer_) {
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    // profile_manager might be NULL in testing environments or during shutdown.
    if (profile_manager)
      profile_manager->GetProfileAttributesStorage().RemoveObserver(this);
  }
}

void AppShortcutManager::OnExtensionWillBeInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update,
    const std::string& old_name) {
  if (!extension->is_app())
    return;

#if defined(OS_MACOSX)
  AppShimRegistry::Get()->OnAppInstalledForProfile(extension->id(),
                                                   profile_->GetPath());
#endif

  // If the app is being updated, update any existing shortcuts but do not
  // create new ones. If it is being installed, automatically create a
  // shortcut in the applications menu (e.g., Start Menu).
  if (is_update) {
    web_app::UpdateAllShortcuts(base::UTF8ToUTF16(old_name), profile_,
                                extension, base::Closure());
  } else {
    CreateShortcutsForApp(profile_, extension);
  }
}

void AppShortcutManager::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
#if defined(OS_MACOSX)
  if (extension->is_app()) {
    AppShimRegistry::Get()->OnAppUninstalledForProfile(extension->id(),
                                                       profile_->GetPath());
    // TODO(https://crbug.com/1001213): Plumb the return result through
    // DeleteAllShortcuts, to appropriately delete multi-profile apps.
  }
#endif

  web_app::DeleteAllShortcuts(profile_, extension);
}

void AppShortcutManager::OnProfileWillBeRemoved(
    const base::FilePath& profile_path) {
  if (profile_path != profile_->GetPath())
    return;

  // TODO(https://crbug.com/1001213): Update AppShimRegistry here.
  web_app::internals::GetShortcutIOTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&web_app::internals::DeleteAllShortcutsForProfile,
                     profile_path));
}

void AppShortcutManager::UpdateShortcutsForAllAppsNow() {
  web_app::UpdateShortcutsForAllApps(
      profile_,
      base::BindOnce(&AppShortcutManager::SetCurrentAppShortcutsVersion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppShortcutManager::SetCurrentAppShortcutsVersion() {
  profile_->GetPrefs()->SetInteger(prefs::kAppShortcutsVersion,
                                   kCurrentAppShortcutsVersion);
}

void AppShortcutManager::UpdateShortcutsForAllAppsIfNeeded() {
  // Updating shortcuts writes to user home folders, which can not be done in
  // tests without exploding disk space usage on the bots.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kTestType))
    return;

  int last_version =
      profile_->GetPrefs()->GetInteger(prefs::kAppShortcutsVersion);
  if (last_version >= kCurrentAppShortcutsVersion)
    return;

  base::PostDelayedTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&AppShortcutManager::UpdateShortcutsForAllAppsNow,
                     weak_ptr_factory_.GetWeakPtr()),
      base::TimeDelta::FromSeconds(kUpdateShortcutsForAllAppsDelay));
}
