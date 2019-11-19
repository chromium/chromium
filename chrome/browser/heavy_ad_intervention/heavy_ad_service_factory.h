// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class HeavyAdService;

// LazyInstance that owns all HeavyAdServices and associates them with
// Profiles.
class HeavyAdServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the HeavyAdService instance for |context|.
  static HeavyAdService* GetForBrowserContext(content::BrowserContext* context);

  // Gets the LazyInstance that owns all HeavyAdServices.
  static HeavyAdServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<HeavyAdServiceFactory>;

  HeavyAdServiceFactory();
  ~HeavyAdServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(HeavyAdServiceFactory);
};

#endif  // CHROME_BROWSER_HEAVY_AD_INTERVENTION_HEAVY_AD_SERVICE_FACTORY_H_
