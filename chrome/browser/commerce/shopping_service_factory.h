// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace commerce {

class ShoppingService;

class ShoppingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  ShoppingServiceFactory(const ShoppingServiceFactory&) = delete;
  ShoppingServiceFactory& operator=(const ShoppingServiceFactory&) = delete;

  static ShoppingServiceFactory* GetInstance();

  static ShoppingService* GetForBrowserContext(
      content::BrowserContext* context);
  static ShoppingService* GetForBrowserContextIfExists(
      content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ShoppingServiceFactory>;

  ShoppingServiceFactory();
  ~ShoppingServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
