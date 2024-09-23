// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_model_handler_provider_factory.h"

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/permissions/prediction_service/prediction_model_handler_provider.h"

// static
PredictionModelHandlerProviderFactory*
PredictionModelHandlerProviderFactory::GetInstance() {
  static base::NoDestructor<PredictionModelHandlerProviderFactory> instance;
  return instance.get();
}

// static
permissions::PredictionModelHandlerProvider*
PredictionModelHandlerProviderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<permissions::PredictionModelHandlerProvider*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

PredictionModelHandlerProviderFactory::PredictionModelHandlerProviderFactory()
    : ProfileKeyedServiceFactory(
          "PredictionModelHandlerProvider",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

PredictionModelHandlerProviderFactory::
    ~PredictionModelHandlerProviderFactory() = default;

std::unique_ptr<KeyedService>
PredictionModelHandlerProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  if (!optimization_guide)
    return nullptr;
  return std::make_unique<permissions::PredictionModelHandlerProvider>(
      optimization_guide);
}

bool PredictionModelHandlerProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
