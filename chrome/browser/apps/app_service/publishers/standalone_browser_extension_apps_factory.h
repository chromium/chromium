// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace apps {

class StandaloneBrowserExtensionApps;

// Singleton that owns all StandaloneBrowserExtensionApps publisher for
// Chrome Apps and associates them with Profiles.
class StandaloneBrowserExtensionAppsFactoryForApp
    : public ProfileKeyedServiceFactory {
 public:
  static StandaloneBrowserExtensionApps* GetForProfile(Profile* profile);

  static StandaloneBrowserExtensionAppsFactoryForApp* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend base::NoDestructor<StandaloneBrowserExtensionAppsFactoryForApp>;

  StandaloneBrowserExtensionAppsFactoryForApp();
  StandaloneBrowserExtensionAppsFactoryForApp(
      const StandaloneBrowserExtensionAppsFactoryForApp&) = delete;
  StandaloneBrowserExtensionAppsFactoryForApp& operator=(
      const StandaloneBrowserExtensionAppsFactoryForApp&) = delete;
  ~StandaloneBrowserExtensionAppsFactoryForApp() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

// Singleton that owns all StandaloneBrowserExtensionApps publisher for
// Extensions and associates them with Profiles.
class StandaloneBrowserExtensionAppsFactoryForExtension
    : public ProfileKeyedServiceFactory {
 public:
  static StandaloneBrowserExtensionApps* GetForProfile(Profile* profile);

  static StandaloneBrowserExtensionAppsFactoryForExtension* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend base::NoDestructor<StandaloneBrowserExtensionAppsFactoryForExtension>;

  StandaloneBrowserExtensionAppsFactoryForExtension();
  StandaloneBrowserExtensionAppsFactoryForExtension(
      const StandaloneBrowserExtensionAppsFactoryForExtension&) = delete;
  StandaloneBrowserExtensionAppsFactoryForExtension& operator=(
      const StandaloneBrowserExtensionAppsFactoryForExtension&) = delete;
  ~StandaloneBrowserExtensionAppsFactoryForExtension() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_
