// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_optimization_guide_decider_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/integrators/optimization_guide/autofill_optimization_guide_decider.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
AutofillOptimizationGuideDecider*
AutofillOptimizationGuideDeciderFactory::GetForProfile(Profile* profile) {
  return static_cast<AutofillOptimizationGuideDecider*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
AutofillOptimizationGuideDeciderFactory*
AutofillOptimizationGuideDeciderFactory::GetInstance() {
  static base::NoDestructor<AutofillOptimizationGuideDeciderFactory> instance;
  return instance.get();
}

AutofillOptimizationGuideDeciderFactory::
    AutofillOptimizationGuideDeciderFactory()
    : ProfileKeyedServiceFactory(
          "AutofillOptimizationGuideDecider",
          ProfileSelections::BuildForRegularAndIncognito()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

AutofillOptimizationGuideDeciderFactory::
    ~AutofillOptimizationGuideDeciderFactory() = default;

std::unique_ptr<KeyedService>
AutofillOptimizationGuideDeciderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  // AutofillOptimizationGuideDecider depends on the optimization guide keyed
  // service, so make sure it is available before creating an
  // AutofillOptimizationGuideDecider.
  if (!optimization_service) {
    return nullptr;
  }

  return std::make_unique<AutofillOptimizationGuideDecider>(
      /*decider=*/optimization_service);
}

}  // namespace autofill
