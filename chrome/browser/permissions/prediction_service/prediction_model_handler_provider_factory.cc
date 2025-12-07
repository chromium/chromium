// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider_factory.h"

#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/permissions/prediction_service/prediction_model_handler_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/passage_embeddings/passage_embedder_model_observer.h"
#include "components/permissions/features.h"

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
  DependsOn(
      passage_embeddings::PassageEmbedderModelObserverFactory::GetInstance());
}

PredictionModelHandlerProviderFactory::
    ~PredictionModelHandlerProviderFactory() = default;

std::unique_ptr<KeyedService>
PredictionModelHandlerProviderFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  VLOG(1) << "[PermissionsAI]: "
             "PredictionModelHandlerProviderFactory::"
             "BuildServiceInstanceForBrowserContext";
  Profile* profile = Profile::FromBrowserContext(context);
  OptimizationGuideKeyedService* optimization_guide =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  if (!optimization_guide) {
    VLOG(1) << "[PermissionsAI]: OptimizationGuideKeyedService not available.";
    return nullptr;
  }
  passage_embeddings::Embedder* passage_embedder = nullptr;
  passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider =
      nullptr;
  if (base::FeatureList::IsEnabled(permissions::features::kPermissionsAIv4)) {
    if (!passage_embeddings::PassageEmbedderModelObserverFactory::GetForProfile(
            profile)) {
      VLOG(1) << "[PermissionsAI]: PassageEmbedderModelObserver not available, "
                 "passage embedder not setup.";
    } else if (auto* passage_embeddings_service_controller =
                   passage_embeddings::
                       ChromePassageEmbeddingsServiceController::Get()) {
      passage_embedder = passage_embeddings_service_controller->GetEmbedder();
      embedder_metadata_provider = passage_embeddings_service_controller;
      VLOG(1)
          << "[PermissionsAI]: PassageEmbeddingsServiceController available, "
             "passage_embedder setup."
          << (passage_embedder ? "true" : "false");
    } else {
      VLOG(1) << "[PermissionsAI]: PassageEmbeddingsServiceController not "
                 "available, passage embedder not setup.";
    }
  } else {
    VLOG(1) << "[PermissionsAI]: "
               "PredictionModelHandlerProviderFactory::"
               "BuildServiceInstanceForBrowserContext PermissionsAIv4 not "
               "enabled, passage embedder not setup.";
  }
  return std::make_unique<permissions::PredictionModelHandlerProvider>(
      optimization_guide, embedder_metadata_provider, passage_embedder);
}

bool PredictionModelHandlerProviderFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}
