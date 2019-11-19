// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_FACTORY_H_

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class NavigationPredictorKeyedService;
class Profile;

// LazyInstance that owns all NavigationPredictorKeyedServices and associates
// them with Profiles.
class NavigationPredictorKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the NavigationPredictorKeyedService instance for |profile|.
  static NavigationPredictorKeyedService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all NavigationPredictorKeyedServices.
  static NavigationPredictorKeyedServiceFactory* GetInstance();

 private:
  friend struct base::LazyInstanceTraitsBase<
      NavigationPredictorKeyedServiceFactory>;

  NavigationPredictorKeyedServiceFactory();
  ~NavigationPredictorKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorKeyedServiceFactory);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_FACTORY_H_
