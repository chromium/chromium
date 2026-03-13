// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility_annotator/content_annotator/content_annotator_service_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/accessibility_annotator/accessibility_annotator_backend_factory.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service_factory.h"
#include "chrome/browser/page_content_annotations/page_embeddings_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/passage_embedder_model_observer_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/accessibility_annotator/content/content_annotator/content_annotator_service.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"

namespace accessibility_annotator {

// static
ContentAnnotatorService* ContentAnnotatorServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ContentAnnotatorService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ContentAnnotatorServiceFactory* ContentAnnotatorServiceFactory::GetInstance() {
  static base::NoDestructor<ContentAnnotatorServiceFactory> instance;
  return instance.get();
}

ContentAnnotatorServiceFactory::ContentAnnotatorServiceFactory()
    : ProfileKeyedServiceFactory(
          "ContentAnnotatorService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
  DependsOn(page_content_annotations::PageContentExtractionServiceFactory::
                GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
  DependsOn(
      page_content_annotations::PageEmbeddingsServiceFactory::GetInstance());
  DependsOn(AccessibilityAnnotatorBackendFactory::GetInstance());
  DependsOn(
      passage_embeddings::PassageEmbedderModelObserverFactory::GetInstance());
}

ContentAnnotatorServiceFactory::~ContentAnnotatorServiceFactory() = default;

std::unique_ptr<KeyedService>
ContentAnnotatorServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kContentAnnotator)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  page_content_annotations::PageContentAnnotationsService*
      page_content_annotations_service =
          PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (!page_content_annotations_service) {
    return nullptr;
  }

  page_content_annotations::PageContentExtractionService*
      page_content_extraction_service = page_content_annotations::
          PageContentExtractionServiceFactory::GetForProfile(profile);
  if (!page_content_extraction_service) {
    return nullptr;
  }

  OptimizationGuideKeyedService* optimization_guide_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_service) {
    return nullptr;
  }

  page_content_annotations::PageEmbeddingsService* page_embeddings_service =
      page_content_annotations::PageEmbeddingsServiceFactory::GetForProfile(
          profile);
  if (!page_embeddings_service) {
    return nullptr;
  }

  AccessibilityAnnotatorBackend* accessibility_annotator_backend =
      AccessibilityAnnotatorBackendFactory::GetForProfile(profile);
  if (!accessibility_annotator_backend) {
    return nullptr;
  }

  auto* passage_embeddings_service_controller =
      passage_embeddings::ChromePassageEmbeddingsServiceController::Get();

  return ContentAnnotatorService::Create(
      *page_content_annotations_service, *page_content_extraction_service,
      *optimization_guide_service, *page_embeddings_service,
      *accessibility_annotator_backend,
      passage_embeddings_service_controller->GetEmbedder(),
      passage_embeddings_service_controller);
}

bool ContentAnnotatorServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace accessibility_annotator
