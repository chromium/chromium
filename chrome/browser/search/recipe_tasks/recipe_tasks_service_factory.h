// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class RecipeTasksService;

// Factory to access the recipe task service for the current profile.
class RecipeTasksServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static RecipeTasksService* GetForProfile(Profile* profile);
  static RecipeTasksServiceFactory* GetInstance();

  RecipeTasksServiceFactory(const RecipeTasksServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<RecipeTasksServiceFactory>;

  RecipeTasksServiceFactory();
  ~RecipeTasksServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SEARCH_RECIPE_TASKS_RECIPE_TASKS_SERVICE_FACTORY_H_
