// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace commerce {

class ShoppingService;

class ShoppingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  ShoppingServiceFactory(const ShoppingServiceFactory&) = delete;
  ShoppingServiceFactory& operator=(const ShoppingServiceFactory&) = delete;

  static ShoppingServiceFactory* GetInstance();

  static ShoppingService* GetForBrowserContext(
      content::BrowserContext* context);
  static ShoppingService* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  // Associates |testing_factory| with |context| and immediately returns the
  // created KeyedService. Since the factory will be used immediately, it may
  // not be empty.
  KeyedService* SetTestingFactoryAndUse(content::BrowserContext* context,
                                        TestingFactory testing_factory);

 private:
  friend class base::NoDestructor<ShoppingServiceFactory>;

  ShoppingServiceFactory();
  ~ShoppingServiceFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_SHOPPING_SERVICE_FACTORY_H_
