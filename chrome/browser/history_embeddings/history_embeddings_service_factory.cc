// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"

#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/keyed_service/core/service_access_type.h"

// static
history_embeddings::HistoryEmbeddingsService*
HistoryEmbeddingsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<history_embeddings::HistoryEmbeddingsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
HistoryEmbeddingsServiceFactory*
HistoryEmbeddingsServiceFactory::GetInstance() {
  static base::NoDestructor<HistoryEmbeddingsServiceFactory> instance;
  return instance.get();
}

HistoryEmbeddingsServiceFactory::HistoryEmbeddingsServiceFactory()
    : ProfileKeyedServiceFactory("HistoryEmbeddingsService",
                                 ProfileSelections::BuildForRegularProfile()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
}

HistoryEmbeddingsServiceFactory::~HistoryEmbeddingsServiceFactory() = default;

std::unique_ptr<KeyedService>
HistoryEmbeddingsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);
  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile, ServiceAccessType::EXPLICIT_ACCESS);
  // The history service is never null; even unit tests build and use one.
  CHECK(history_service);

  auto* page_content_annotations_service =
      PageContentAnnotationsServiceFactory::GetForProfile(profile);
  if (!page_content_annotations_service) {
    return nullptr;
  }

  return std::make_unique<history_embeddings::HistoryEmbeddingsService>(
      history_service, page_content_annotations_service);
}
