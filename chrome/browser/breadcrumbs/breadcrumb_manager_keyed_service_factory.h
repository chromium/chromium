// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace breadcrumbs {
class BreadcrumbManagerKeyedService;
}  // namespace breadcrumbs

namespace content {
class BrowserContext;
}  // namespace content

class BreadcrumbManagerKeyedServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static BreadcrumbManagerKeyedServiceFactory* GetInstance();

  // Returns the keyed service for `context`. Creates the service if it doesn't
  // already exist.
  static breadcrumbs::BreadcrumbManagerKeyedService* GetForBrowserContext(
      content::BrowserContext* context);

  BreadcrumbManagerKeyedServiceFactory(
      const BreadcrumbManagerKeyedServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<BreadcrumbManagerKeyedServiceFactory>;

  BreadcrumbManagerKeyedServiceFactory();
  ~BreadcrumbManagerKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_BREADCRUMBS_BREADCRUMB_MANAGER_KEYED_SERVICE_FACTORY_H_
