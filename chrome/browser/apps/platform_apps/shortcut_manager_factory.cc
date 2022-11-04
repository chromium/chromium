// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/shortcut_manager_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/os_integration/web_app_shortcut_manager.h"

// static
AppShortcutManager* AppShortcutManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<AppShortcutManager*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 false /* don't create */));
}

AppShortcutManagerFactory* AppShortcutManagerFactory::GetInstance() {
  return base::Singleton<AppShortcutManagerFactory>::get();
}

AppShortcutManagerFactory::AppShortcutManagerFactory()
    : ProfileKeyedServiceFactory("AppShortcutManager") {
  web_app::WebAppShortcutManager::SetUpdateShortcutsForAllAppsCallback(
      base::BindRepeating(&web_app::UpdateShortcutsForAllApps));
}

AppShortcutManagerFactory::~AppShortcutManagerFactory() {}

KeyedService* AppShortcutManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new AppShortcutManager(static_cast<Profile*>(profile));
}

bool AppShortcutManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AppShortcutManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
