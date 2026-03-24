// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class OptimizationGuideGlobalStateHolderKeyedService;
class Profile;

class OptimizationGuideGlobalStateHolderKeyedServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets the `OptimizationGuideGlobalStateHolderKeyedService` for the profile.
  static OptimizationGuideGlobalStateHolderKeyedService* GetForProfile(
      Profile* profile);
  // Gets the singleton factory instance.
  static OptimizationGuideGlobalStateHolderKeyedServiceFactory* GetInstance();

  OptimizationGuideGlobalStateHolderKeyedServiceFactory(
      const OptimizationGuideGlobalStateHolderKeyedServiceFactory&) = delete;
  OptimizationGuideGlobalStateHolderKeyedServiceFactory& operator=(
      const OptimizationGuideGlobalStateHolderKeyedServiceFactory&) = delete;

 private:
  friend base::NoDestructor<
      OptimizationGuideGlobalStateHolderKeyedServiceFactory>;

  OptimizationGuideGlobalStateHolderKeyedServiceFactory();
  ~OptimizationGuideGlobalStateHolderKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_GLOBAL_STATE_HOLDER_KEYED_SERVICE_FACTORY_H_
