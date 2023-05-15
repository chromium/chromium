// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_validator_keyed_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/model_validator_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "content/public/browser/browser_context.h"

namespace optimization_guide {

// static
ModelValidatorKeyedServiceFactory*
ModelValidatorKeyedServiceFactory::GetInstance() {
  DCHECK(switches::ShouldValidateModel());
  static base::NoDestructor<ModelValidatorKeyedServiceFactory> factory;
  return factory.get();
}

ModelValidatorKeyedServiceFactory::ModelValidatorKeyedServiceFactory()
    : ProfileKeyedServiceFactory(
          "ModelValidatorKeyedService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DCHECK(switches::ShouldValidateModel());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

ModelValidatorKeyedServiceFactory::~ModelValidatorKeyedServiceFactory() =
    default;

KeyedService* ModelValidatorKeyedServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ModelValidatorKeyedService(Profile::FromBrowserContext(context));
}

bool ModelValidatorKeyedServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return switches::ShouldValidateModel();
}

}  // namespace optimization_guide
