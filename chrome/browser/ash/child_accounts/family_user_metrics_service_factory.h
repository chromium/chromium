// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class FamilyUserMetricsService;

// Singleton that owns FamilyUserMetricsService object and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// FamilyUserMetricsService.
class FamilyUserMetricsServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static FamilyUserMetricsService* GetForBrowserContext(
      content::BrowserContext* context);

  static FamilyUserMetricsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<FamilyUserMetricsServiceFactory>;

  FamilyUserMetricsServiceFactory();
  FamilyUserMetricsServiceFactory(const FamilyUserMetricsServiceFactory&) =
      delete;
  FamilyUserMetricsServiceFactory& operator=(
      const FamilyUserMetricsServiceFactory&) = delete;

  ~FamilyUserMetricsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_FAMILY_USER_METRICS_SERVICE_FACTORY_H_
