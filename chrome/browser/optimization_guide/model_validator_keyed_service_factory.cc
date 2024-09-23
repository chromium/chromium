// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "content/public/browser/browser_context.h"

namespace optimization_guide {

// static
ModelValidatorKeyedServiceFactory*
ModelValidatorKeyedServiceFactory::GetInstance() {
  DCHECK(ShouldStartModelValidator());
  static base::NoDestructor<ModelValidatorKeyedServiceFactory> factory;
  return factory.get();
}

ModelValidatorKeyedServiceFactory::ModelValidatorKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "ModelValidatorKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DCHECK(ShouldStartModelValidator());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

ModelValidatorKeyedServiceFactory::~ModelValidatorKeyedServiceFactory() =
    default;

std::unique_ptr<KeyedService>
  ModelValidatorKeyedServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ModelValidatorKeyedService>(
      Profile::FromBrowserContext(context));
}

bool ModelValidatorKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return ShouldStartModelValidator();
}

}  // namespace optimization_guide
