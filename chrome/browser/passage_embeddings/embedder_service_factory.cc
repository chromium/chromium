// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/embedder_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/passage_embeddings/embedder_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/passage_embeddings/passage_embeddings_features.h"

namespace passage_embeddings {

// static
EmbedderService* EmbedderServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<EmbedderService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
EmbedderServiceFactory* EmbedderServiceFactory::GetInstance() {
  static base::NoDestructor<EmbedderServiceFactory> instance;
  return instance.get();
}

EmbedderServiceFactory::EmbedderServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryEmbeddingsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

EmbedderServiceFactory::~EmbedderServiceFactory() = default;

std::unique_ptr<KeyedService>
EmbedderServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  if (!base::FeatureList::IsEnabled(kPassageEmbedder)) {
    return nullptr;
  }

  return std::make_unique<EmbedderService>(
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      ChromePassageEmbeddingsServiceController::Get());
}

}  // namespace passage_embeddings
