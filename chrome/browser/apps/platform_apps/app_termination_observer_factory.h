// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_FACTORY_H_
#define CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chrome_apps {

class AppTerminationObserverFactory : public ProfileKeyedServiceFactory {
 public:
  // Note: this factory currently has no
  // AppTerminationObserver* GetForProfile(Profile*) because the observer has no
  // methods to call.

  static AppTerminationObserverFactory* GetInstance();

  AppTerminationObserverFactory(const AppTerminationObserverFactory&) = delete;
  AppTerminationObserverFactory& operator=(
      const AppTerminationObserverFactory&) = delete;

 private:
  friend base::NoDestructor<AppTerminationObserverFactory>;

  AppTerminationObserverFactory();
  ~AppTerminationObserverFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chrome_apps

#endif  // CHROME_BROWSER_APPS_PLATFORM_APPS_APP_TERMINATION_OBSERVER_FACTORY_H_
