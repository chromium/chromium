// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/common/extension.h"

class Profile;

// This class manages the installation of shortcuts for any extension-based apps
// (Chrome Apps). Bookmark apps OS shortcut management is handled in
// web_app::AppShortcutManager and its subclasses.
//
// Long term, this class must be deleted together with all extension-based apps.
class AppShortcutManager : public KeyedService,
                           public extensions::ExtensionRegistryObserver,
                           public ProfileAttributesStorage::Observer {
 public:
  explicit AppShortcutManager(Profile* profile);

  AppShortcutManager(const AppShortcutManager&) = delete;
  AppShortcutManager& operator=(const AppShortcutManager&) = delete;
  ~AppShortcutManager() override;

  // extensions::ExtensionRegistryObserver.
  void OnExtensionWillBeInstalled(content::BrowserContext* browser_context,
                                  const extensions::Extension* extension,
                                  bool is_update,
                                  const std::string& old_name) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

  // ProfileAttributesStorage::Observer.
  void OnProfileWillBeRemoved(const base::FilePath& profile_path) override;

  static void SuppressShortcutsForTesting();

 private:
  void DeleteApplicationShortcuts(const extensions::Extension* extension);

  raw_ptr<Profile> profile_;
  base::ScopedObservation<ProfileAttributesStorage,
                          ProfileAttributesStorage::Observer>
      profile_storage_observation_{this};
  base::ScopedObservation<extensions::ExtensionRegistry,
                          extensions::ExtensionRegistryObserver>
      extension_registry_observation_{this};

  base::WeakPtrFactory<AppShortcutManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_H_
