// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMMERCE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace commerce {
class ProductSpecificationsService;

// Factory method to acquire ProductSpecificationService which is BrowserContext
// keyed.
class ProductSpecificationsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static commerce::ProductSpecificationsService* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static ProductSpecificationsServiceFactory* GetInstance();

  ProductSpecificationsServiceFactory(
      const ProductSpecificationsServiceFactory&) = delete;
  ProductSpecificationsServiceFactory& operator=(
      const ProductSpecificationsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ProductSpecificationsServiceFactory>;

  ProductSpecificationsServiceFactory();
  ~ProductSpecificationsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_PRODUCT_SPECIFICATIONS_PRODUCT_SPECIFICATIONS_SERVICE_FACTORY_H_
