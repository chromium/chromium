// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/page_content_annotations/content/embeddings_candidate_generator.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"

namespace page_content_annotations {

// static
PageEmbeddingsService* PageEmbeddingsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<PageEmbeddingsService*>(
      GetInstance()->GetServiceForBrowserContext(/*context=*/profile,
                                                 /*create=*/true));
}

// static
PageEmbeddingsServiceFactory* PageEmbeddingsServiceFactory::GetInstance() {
  static base::NoDestructor<PageEmbeddingsServiceFactory> instance;
  return instance.get();
}

PageEmbeddingsServiceFactory::PageEmbeddingsServiceFactory()
    : ProfileKeyedServiceFactory(
          "PageEmbeddingsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PageContentExtractionServiceFactory::GetInstance());
  DependsOn(
      passage_embeddings::PassageEmbedderModelObserverFactory::GetInstance());
}

PageEmbeddingsServiceFactory::~PageEmbeddingsServiceFactory() = default;

std::unique_ptr<KeyedService>
PageEmbeddingsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  Profile* profile = Profile::FromBrowserContext(browser_context);

  // Don't bother running if we don't have a model observer since we won't have
  // a model to run.
  if (!passage_embeddings::PassageEmbedderModelObserverFactory::GetForProfile(
          profile)) {
    return nullptr;
  }

  auto* page_content_extraction_service =
      PageContentExtractionServiceFactory::GetForProfile(profile);
  if (!page_content_extraction_service) {
    return nullptr;
  }

  return std::make_unique<PageEmbeddingsService>(
      base::BindRepeating(&GenerateEmbeddingsCandidates),
      page_content_extraction_service,
      passage_embeddings::ChromePassageEmbeddingsServiceController::Get()
          ->GetEmbedder(),
      passage_embeddings::ChromePassageEmbeddingsServiceController::Get());
}

bool PageEmbeddingsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace page_content_annotations
