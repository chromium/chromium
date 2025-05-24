// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/chrome_history_embeddings_service.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/history_query.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace history_embeddings {

ChromeHistoryEmbeddingsService::ChromeHistoryEmbeddingsService(
    Profile* profile,
    history::HistoryService* history_service,
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder,
    std::unique_ptr<Answerer> answerer,
    std::unique_ptr<IntentClassifier> intent_classifier)
    : HistoryEmbeddingsService(g_browser_process->os_crypt_async(),
                               history_service,
                               page_content_annotations_service,
                               optimization_guide_decider,
                               embedder_metadata_provider,
                               embedder,
                               std::move(answerer),
                               std::move(intent_classifier)),
      profile_(profile) {}

ChromeHistoryEmbeddingsService::~ChromeHistoryEmbeddingsService() = default;

bool ChromeHistoryEmbeddingsService::IsAnswererUseAllowed() const {
  return IsHistoryEmbeddingsAnswersEnabledForProfile(profile_);
}

QualityLogEntry ChromeHistoryEmbeddingsService::PrepareQualityLogEntry() {
  OptimizationGuideKeyedService* optimization_guide_keyed_service =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile_);
  if (!optimization_guide_keyed_service) {
    return nullptr;
  }

  auto* quality_uploader =
      optimization_guide_keyed_service->GetModelQualityLogsUploaderService();
  if (!quality_uploader) {
    return nullptr;
  }

  QualityLogEntry log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          quality_uploader->GetWeakPtr());

  optimization_guide::proto::LogAiDataRequest* request =
      log_entry->log_ai_data_request();
  if (!request) {
    return nullptr;
  }

  return log_entry;
}

}  // namespace history_embeddings
