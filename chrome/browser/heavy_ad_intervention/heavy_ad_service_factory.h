// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace heavy_ad_intervention {
class HeavyAdService;
}

// LazyInstance that owns all HeavyAdServices and associates them with
// Profiles.
class HeavyAdServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the HeavyAdService instance for |context|.
  static heavy_ad_intervention::HeavyAdService* GetForBrowserContext(
      content::BrowserContext* context);

  // Gets the LazyInstance that owns all HeavyAdServices.
  static HeavyAdServiceFactory* GetInstance();

  HeavyAdServiceFactory(const HeavyAdServiceFactory&) = delete;
  HeavyAdServiceFactory& operator=(const HeavyAdServiceFactory&) = delete;

 private:
  friend struct base::LazyInstanceTraitsBase<HeavyAdServiceFactory>;

  HeavyAdServiceFactory();
  ~HeavyAdServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_
