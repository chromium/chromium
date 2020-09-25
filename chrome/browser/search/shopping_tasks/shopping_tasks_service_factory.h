// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class ShoppingTasksService;

// Factory to access the shopping task service for the current profile.
class ShoppingTasksServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ShoppingTasksService* GetForProfile(Profile* profile);
  static ShoppingTasksServiceFactory* GetInstance();

  ShoppingTasksServiceFactory(const ShoppingTasksServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<ShoppingTasksServiceFactory>;

  ShoppingTasksServiceFactory();
  ~ShoppingTasksServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SEARCH_SHOPPING_TASKS_SHOPPING_TASKS_SERVICE_FACTORY_H_
