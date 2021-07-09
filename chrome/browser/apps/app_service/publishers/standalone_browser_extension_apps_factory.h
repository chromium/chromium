// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class StandaloneBrowserExtensionApps;

// Singleton that owns all StandaloneBrowserExtensionApps publisher and
// associates them with Profiles.
class StandaloneBrowserExtensionAppsFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static StandaloneBrowserExtensionApps* GetForProfile(Profile* profile);

  static StandaloneBrowserExtensionAppsFactory* GetInstance();

  static void ShutDownForTesting(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<
      StandaloneBrowserExtensionAppsFactory>;

  StandaloneBrowserExtensionAppsFactory();
  StandaloneBrowserExtensionAppsFactory(
      const StandaloneBrowserExtensionAppsFactory&) = delete;
  StandaloneBrowserExtensionAppsFactory& operator=(
      const StandaloneBrowserExtensionAppsFactory&) = delete;
  ~StandaloneBrowserExtensionAppsFactory() override = default;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_PUBLISHERS_STANDALONE_BROWSER_EXTENSION_APPS_FACTORY_H_
