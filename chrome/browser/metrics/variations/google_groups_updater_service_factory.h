// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

class GoogleGroupsUpdaterService;

class GoogleGroupsUpdaterServiceFactory : public ProfileKeyedServiceFactory {
 public:
  GoogleGroupsUpdaterServiceFactory();

  static GoogleGroupsUpdaterServiceFactory* GetInstance();
  static GoogleGroupsUpdaterService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  // `BrowserContextKeyedServiceFactory` overrides.
  //
  // Returns nullptr if `OptimizationGuideKeyedService` is null.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_GOOGLE_GROUPS_UPDATER_SERVICE_FACTORY_H_
