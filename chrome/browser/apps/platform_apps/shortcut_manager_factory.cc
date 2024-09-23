// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/shortcut_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/apps/platform_apps/shortcut_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"

// static
AppShortcutManager* AppShortcutManagerFactory::GetForProfile(Profile* profile) {
  return static_cast<AppShortcutManager*>(
      GetInstance()->GetServiceForBrowserContext(profile,
                                                 false /* don't create */));
}

AppShortcutManagerFactory* AppShortcutManagerFactory::GetInstance() {
  static base::NoDestructor<AppShortcutManagerFactory> instance;
  return instance.get();
}

AppShortcutManagerFactory::AppShortcutManagerFactory()
    : ProfileKeyedServiceFactory(
          "AppShortcutManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  web_app::OsIntegrationManager::SetUpdateShortcutsForAllAppsCallback(
      base::BindRepeating(&web_app::UpdateShortcutsForAllApps));
}

AppShortcutManagerFactory::~AppShortcutManagerFactory() = default;

std::unique_ptr<KeyedService>
AppShortcutManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile)
    return nullptr;

  // Do not instantiate the AppShortcutManager if web_apps are not supported.
  if (!web_app::AreWebAppsEnabled(profile))
    return nullptr;

  return std::make_unique<AppShortcutManager>(profile);
}

bool AppShortcutManagerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AppShortcutManagerFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
