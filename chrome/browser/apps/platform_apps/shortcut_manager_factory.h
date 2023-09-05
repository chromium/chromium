// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

class AppShortcutManager;

// Singleton that owns all AppShortcutManagers and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated AppShortcutManager.
// AppShortcutManager should only exist for profiles where web apps are enabled.
class AppShortcutManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static AppShortcutManager* GetForProfile(Profile* profile);

  static AppShortcutManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<AppShortcutManagerFactory>;

  AppShortcutManagerFactory();
  ~AppShortcutManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};
#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_SHORTCUT_MANAGER_FACTORY_H_
