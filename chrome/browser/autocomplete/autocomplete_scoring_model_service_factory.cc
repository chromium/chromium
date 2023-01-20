// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

// static
AutocompleteScoringModelServiceFactory*
AutocompleteScoringModelServiceFactory::GetInstance() {
  return base::Singleton<AutocompleteScoringModelServiceFactory>::get();
}

// static
AutocompleteScoringModelService*
AutocompleteScoringModelServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AutocompleteScoringModelService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AutocompleteScoringModelServiceFactory::AutocompleteScoringModelServiceFactory()
    : ProfileKeyedServiceFactory("AutocompleteScoringModelService") {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutocompleteScoringModelServiceFactory::
    ~AutocompleteScoringModelServiceFactory() = default;

std::unique_ptr<KeyedService>
AutocompleteScoringModelServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide ? std::make_unique<AutocompleteScoringModelService>(
                                  optimization_guide)
                            : nullptr;
}

bool AutocompleteScoringModelServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool AutocompleteScoringModelServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
