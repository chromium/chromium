// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/history_embeddings_service_factory.h"

#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_embeddings/chrome_history_embeddings_service.h"
#include "chrome/browser/history_embeddings/chrome_passage_embeddings_service_controller.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/page_content_annotations/page_content_annotations_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "components/history_embeddings/ml_answerer.h"
#include "components/history_embeddings/ml_embedder.h"
#include "components/history_embeddings/ml_intent_classifier.h"
#include "components/history_embeddings/mock_answerer.h"
#include "components/history_embeddings/mock_intent_classifier.h"
#include "components/keyed_service/core/service_access_type.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace {

bool IsEphemeralProfile(Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::ProfileHelper::IsEphemeralUserProfile(profile)) {
    return true;
  }
#endif

  // Catch additional logic that may not be caught by the existing Ash check.
  ProfileAttributesStorage& storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  ProfileAttributesEntry* entry =
      storage.GetProfileAttributesWithPath(profile->GetPath());
  return entry && entry->IsEphemeral();
}

bool ShouldBuildServiceInstance(Profile* profile) {
  // Do NOT construct the service if the feature flag is disabled.
  if (!history_embeddings::IsHistoryEmbeddingsEnabled()) {
    return false;
  }

  // Embeddings don't last long enough to help users in kiosk or ephemeral
  // profile mode, so simply never construct the service for those users.
  if (IsRunningInAppMode() || IsEphemeralProfile(profile)) {
    return false;
  }

  return true;
}

}  // namespace

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

// static
std::unique_ptr<KeyedService> HistoryEmbeddingsServiceFactory::
    BuildServiceInstanceForBrowserContextForTesting(
        content::BrowserContext* context,
        std::unique_ptr<history_embeddings::Embedder> embedder,
        std::unique_ptr<history_embeddings::Answerer> answerer,
        std::unique_ptr<history_embeddings::IntentClassifier>
            intent_classifier) {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!ShouldBuildServiceInstance(profile)) {
    return nullptr;
  }

  return std::make_unique<history_embeddings::ChromeHistoryEmbeddingsService>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      PageContentAnnotationsServiceFactory::GetForProfile(profile),
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile),
      std::move(embedder), std::move(answerer), std::move(intent_classifier));
}

HistoryEmbeddingsServiceFactory::HistoryEmbeddingsServiceFactory()
    : ProfileKeyedServiceFactory(
          "HistoryEmbeddingsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HistoryServiceFactory::GetInstance());
  DependsOn(PageContentAnnotationsServiceFactory::GetInstance());
  DependsOn(OptimizationGuideKeyedServiceFactory::GetInstance());
}

HistoryEmbeddingsServiceFactory::~HistoryEmbeddingsServiceFactory() = default;

std::unique_ptr<KeyedService>
HistoryEmbeddingsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!ShouldBuildServiceInstance(profile)) {
    return nullptr;
  }

  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);

  std::unique_ptr<history_embeddings::Answerer> answerer;
  if (history_embeddings::IsHistoryEmbeddingsAnswersEnabled()) {
    if (history_embeddings::kUseMlAnswerer.Get()) {
      answerer = std::make_unique<history_embeddings::MlAnswerer>(
          optimization_guide_keyed_service);
    } else {
      answerer = std::make_unique<history_embeddings::MockAnswerer>();
    }
  }

  std::unique_ptr<history_embeddings::IntentClassifier> intent_classifier;
  if (history_embeddings::kEnableIntentClassifier.Get()) {
    if (history_embeddings::kUseMlIntentClassifier.Get()) {
      intent_classifier =
          std::make_unique<history_embeddings::MlIntentClassifier>(
              optimization_guide_keyed_service);
    } else {
      intent_classifier =
          std::make_unique<history_embeddings::MockIntentClassifier>();
    }
  }

  return std::make_unique<history_embeddings::ChromeHistoryEmbeddingsService>(
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      PageContentAnnotationsServiceFactory::GetForProfile(profile),
      optimization_guide_keyed_service,
      std::make_unique<history_embeddings::MlEmbedder>(
          optimization_guide_keyed_service,
          history_embeddings::ChromePassageEmbeddingsServiceController::Get()),
      std::move(answerer), std::move(intent_classifier));
}
