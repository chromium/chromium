// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_context.h"

// static
OptimizationGuideKeyedService*
OptimizationGuideKeyedServiceFactory::GetForProfile(Profile* profile) {
  if (optimization_guide::features::IsOptimizationHintsEnabled()) {
    return static_cast<OptimizationGuideKeyedService*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
OptimizationGuideKeyedServiceFactory*
OptimizationGuideKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<OptimizationGuideKeyedServiceFactory> factory;
  return factory.get();
}

OptimizationGuideKeyedServiceFactory::OptimizationGuideKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "OptimizationGuideKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // Guest Profile follows Regular Profile selection mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // Do not build the OptimizationGuideKeyedService if it's a
              // sign-in or lockscreen profile since it basically is an
              // ephemeral profile anyway and we cannot provide hints or models
              // to it anyway.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
}

OptimizationGuideKeyedServiceFactory::~OptimizationGuideKeyedServiceFactory() =
    default;

KeyedService* OptimizationGuideKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OptimizationGuideKeyedService(context);
}

bool OptimizationGuideKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return optimization_guide::features::IsOptimizationHintsEnabled();
}

bool OptimizationGuideKeyedServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
