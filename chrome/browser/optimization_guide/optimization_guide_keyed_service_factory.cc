// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"

#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/optimization_guide/optimization_guide_features.h"
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
    : BrowserContextKeyedServiceFactory(
          "OptimizationGuideKeyedService",
          BrowserContextDependencyManager::GetInstance()) {}

OptimizationGuideKeyedServiceFactory::~OptimizationGuideKeyedServiceFactory() =
    default;

KeyedService* OptimizationGuideKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new OptimizationGuideKeyedService(context);
}
