// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/passage_embeddings_coordinator_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/passage_embeddings/passage_embeddings_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/passage_embeddings/passage_embeddings_features.h"

namespace passage_embeddings {

// static
PassageEmbeddingsCoordinator*
PassageEmbeddingsCoordinatorFactory::GetForProfile(Profile* profile) {
  return static_cast<PassageEmbeddingsCoordinator*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
PassageEmbeddingsCoordinatorFactory*
PassageEmbeddingsCoordinatorFactory::GetInstance() {
  static base::NoDestructor<PassageEmbeddingsCoordinatorFactory> instance;
  return instance.get();
}

PassageEmbeddingsCoordinatorFactory::PassageEmbeddingsCoordinatorFactory()
    : ProfileKeyedServiceFactory(
          "PassageEmbeddingsCoordinator",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(page_content_annotations::PageContentExtractionServiceFactory::
                GetInstance());
  DependsOn(PassageEmbedderModelObserverFactory::GetInstance());
}

PassageEmbeddingsCoordinatorFactory::~PassageEmbeddingsCoordinatorFactory() =
    default;

std::unique_ptr<KeyedService>
PassageEmbeddingsCoordinatorFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* profile) const {
  // Don't run the experiment for clients with history embeddings enabled.
  if (history_embeddings::IsHistoryEmbeddingsEnabledForProfile(
          Profile::FromBrowserContext(profile))) {
    return nullptr;
  }

  // Don't bother running if we don't have a model observer since we won't have
  // a model to run.
  if (!PassageEmbedderModelObserverFactory::GetForProfile(
          Profile::FromBrowserContext(profile))) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(passage_embeddings::kPassageEmbedder)) {
    return nullptr;
  }

  // Required to ensure the model observer starts.
  PassageEmbedderModelObserverFactory::GetForProfile(
      Profile::FromBrowserContext(profile));

  return std::make_unique<PassageEmbeddingsCoordinator>(
      page_content_annotations::PageContentExtractionServiceFactory::
          GetForProfile(Profile::FromBrowserContext(profile)));
}

bool PassageEmbeddingsCoordinatorFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace passage_embeddings
