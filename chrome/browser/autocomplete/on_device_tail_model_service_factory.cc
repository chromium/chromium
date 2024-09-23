// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/on_device_tail_model_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/omnibox/browser/on_device_tail_model_service.h"

// static
OnDeviceTailModelServiceFactory*
OnDeviceTailModelServiceFactory::GetInstance() {
  static base::NoDestructor<OnDeviceTailModelServiceFactory> instance;
  return instance.get();
}

// static
OnDeviceTailModelService* OnDeviceTailModelServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<OnDeviceTailModelService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

OnDeviceTailModelServiceFactory::OnDeviceTailModelServiceFactory()
    : ProfileKeyedServiceFactory(
          "OnDeviceTailModelService",
          // This service will be accessible for both regular and guest profile
          // in both original and OTR mode.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

OnDeviceTailModelServiceFactory::~OnDeviceTailModelServiceFactory() = default;

std::unique_ptr<KeyedService>
OnDeviceTailModelServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!OmniboxFieldTrial::IsOnDeviceTailSuggestEnabled()) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  return optimization_guide
             ? std::make_unique<OnDeviceTailModelService>(optimization_guide)
             : nullptr;
}

bool OnDeviceTailModelServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool OnDeviceTailModelServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
