// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_global_state_holder_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"

// static
OptimizationGuideGlobalStateHolderKeyedService*
OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetForProfile(
    Profile* profile) {
  // TODO: crbug.com/481734987 - remove this check to decouple hints and model
  // delivery.
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    return static_cast<OptimizationGuideGlobalStateHolderKeyedService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
OptimizationGuideGlobalStateHolderKeyedServiceFactory*
OptimizationGuideGlobalStateHolderKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<
      OptimizationGuideGlobalStateHolderKeyedServiceFactory>
      factory;
  return factory.get();
}

OptimizationGuideGlobalStateHolderKeyedServiceFactory::
    OptimizationGuideGlobalStateHolderKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "OptimizationGuideGlobalStateHolderKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

OptimizationGuideGlobalStateHolderKeyedServiceFactory::
    ~OptimizationGuideGlobalStateHolderKeyedServiceFactory() = default;

std::unique_ptr<KeyedService>
OptimizationGuideGlobalStateHolderKeyedServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<OptimizationGuideGlobalStateHolderKeyedService>();
}

bool OptimizationGuideGlobalStateHolderKeyedServiceFactory::
    ServiceIsNULLWhileTesting() const {
  return true;
}
