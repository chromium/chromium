// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class OptimizationGuideKeyedService;
class Profile;

// LazyInstance that owns all OptimizationGuideKeyedServices and associates them
// with Profiles.
class OptimizationGuideKeyedServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Gets the OptimizationGuideKeyedService for the profile.
  //
  // Returns null if the features that allow for this to provide useful
  // information are disabled.
  static OptimizationGuideKeyedService* GetForProfile(Profile* profile);

  // Gets the LazyInstance that owns all OptimizationGuideKeyedService(s).
  static OptimizationGuideKeyedServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<OptimizationGuideKeyedServiceFactory>;

  OptimizationGuideKeyedServiceFactory();
  ~OptimizationGuideKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(OptimizationGuideKeyedServiceFactory);
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_KEYED_SERVICE_FACTORY_H_
