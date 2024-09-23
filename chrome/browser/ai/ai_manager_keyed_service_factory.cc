// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ai/ai_manager_keyed_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ai/ai_manager_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/browser_context.h"

// static
AIManagerKeyedService* AIManagerKeyedServiceFactory::GetAIManagerKeyedService(
    content::BrowserContext* browser_context) {
  return static_cast<AIManagerKeyedService*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
AIManagerKeyedServiceFactory* AIManagerKeyedServiceFactory::GetInstance() {
  static base::NoDestructor<AIManagerKeyedServiceFactory> factory;
  return factory.get();
}

AIManagerKeyedServiceFactory::AIManagerKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "AIManagerKeyedService",
          // Mostly following `OptimizationGuideKeyedServiceFactory`
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // There is no original type for guest profile, so we use
              // `kOffTheRecordOnly`.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AIManagerKeyedServiceFactory::~AIManagerKeyedServiceFactory() = default;

bool AIManagerKeyedServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
AIManagerKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AIManagerKeyedService>(context);
}
