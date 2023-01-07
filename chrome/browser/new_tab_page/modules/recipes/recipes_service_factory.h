// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class RecipesService;

// Factory to access the recipe module service for the current profile.
class RecipesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static RecipesService* GetForProfile(Profile* profile);
  static RecipesServiceFactory* GetInstance();

  RecipesServiceFactory(const RecipesServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<RecipesServiceFactory>;

  RecipesServiceFactory();
  ~RecipesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_RECIPES_RECIPES_SERVICE_FACTORY_H_
