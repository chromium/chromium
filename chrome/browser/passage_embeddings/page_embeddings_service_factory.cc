// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/page_embeddings_service_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/embeddings_candidate_generator.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/passage_embeddings/passage_embeddings_features.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace passage_embeddings {

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
  DependsOn(page_content_annotations::PageContentExtractionServiceFactory::
                GetInstance());
  DependsOn(PassageEmbedderModelObserverFactory::GetInstance());
}

PageEmbeddingsServiceFactory::~PageEmbeddingsServiceFactory() = default;

std::unique_ptr<KeyedService>
PageEmbeddingsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
#if BUILDFLAG(IS_CHROMEOS)
  if (!base::FeatureList::IsEnabled(
          chromeos::features::kFeatureManagementPassageEmbedder)) {
    return nullptr;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  Profile* profile = Profile::FromBrowserContext(browser_context);
  // Don't run the experiment for clients with history embeddings enabled.
  if (history_embeddings::IsHistoryEmbeddingsEnabledForProfile(profile)) {
    return nullptr;
  }

  // Don't bother running if we don't have a model observer since we won't have
  // a model to run.
  if (!PassageEmbedderModelObserverFactory::GetForProfile(profile)) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(passage_embeddings::kPassageEmbedder)) {
    return nullptr;
  }

  // Required to ensure the model observer starts.
  PassageEmbedderModelObserverFactory::GetForProfile(profile);

  auto* page_content_extraction_service = page_content_annotations::
      PageContentExtractionServiceFactory::GetForProfile(profile);
  if (!page_content_extraction_service) {
    return nullptr;
  }

  return std::make_unique<PageEmbeddingsService>(
      base::BindRepeating(&GenerateEmbeddingsCandidates),
      page_content_extraction_service,
      ChromePassageEmbeddingsServiceController::Get()->GetEmbedder());
}

bool PageEmbeddingsServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace passage_embeddings
