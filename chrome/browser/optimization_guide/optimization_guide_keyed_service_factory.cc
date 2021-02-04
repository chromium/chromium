// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

#include "build/build_config.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

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
    : BrowserContextKeyedServiceFactory(
          "OptimizationGuideKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

OptimizationGuideKeyedServiceFactory::~OptimizationGuideKeyedServiceFactory() =
    default;

KeyedService* OptimizationGuideKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Do not build the OptimizationGuideKeyedService if it's a sign-in profile
  // since it basically is an ephemeral profile anyway and we cannot provide
  // hints or models to it anyway. Additionally, sign in profiles do not go
  // through the standard profile initialization flow, so a lot of things that
  // are required are not available when the browser context for the signin
  // profile is created.
  Profile* profile = Profile::FromBrowserContext(context);
  if (chromeos::ProfileHelper::IsSigninProfile(profile))
    return nullptr;
#endif

  return new OptimizationGuideKeyedService(context);
}

content::BrowserContext*
OptimizationGuideKeyedServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}

bool OptimizationGuideKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return optimization_guide::features::IsOptimizationHintsEnabled();
}

bool OptimizationGuideKeyedServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
