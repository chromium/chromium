// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_scoring_model_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/autocomplete_scoring_model_service.h"
#include "components/omnibox/browser/omnibox_field_trial.h"

// static
AutocompleteScoringModelServiceFactory*
AutocompleteScoringModelServiceFactory::GetInstance() {
  static base::NoDestructor<AutocompleteScoringModelServiceFactory> instance;
  return instance.get();
}

// static
AutocompleteScoringModelService*
AutocompleteScoringModelServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AutocompleteScoringModelService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

AutocompleteScoringModelServiceFactory::AutocompleteScoringModelServiceFactory()
    : ProfileKeyedServiceFactory(
          "AutocompleteScoringModelService",
          // This service is available for the regular profile in both the
          // original and the OTR modes.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode (likely not since local history is unavailable).
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
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
