// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history_embeddings/chrome_history_embeddings_service.h"

#include <memory>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/optimization_guide/chrome_model_quality_logs_uploader_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "components/optimization_guide/core/model_quality/feature_type_map.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/history_document.pb.h"
#include "components/optimization_guide/proto/features/history_query.pb.h"
#include "components/optimization_guide/proto/model_quality_service.pb.h"

namespace history_embeddings {

ChromeHistoryEmbeddingsService::ChromeHistoryEmbeddingsService(
    history::HistoryService* history_service,
    page_content_annotations::PageContentAnnotationsService*
        page_content_annotations_service,
    OptimizationGuideKeyedService* optimization_guide_service,
    PassageEmbeddingsServiceController* service_controller)
    : HistoryEmbeddingsService(history_service,
                               page_content_annotations_service,
                               optimization_guide_service,
                               optimization_guide_service,
                               service_controller,
                               g_browser_process->os_crypt_async(),
                               optimization_guide_service),
      optimization_guide_service_(optimization_guide_service) {}

ChromeHistoryEmbeddingsService::~ChromeHistoryEmbeddingsService() = default;

QualityLogEntry ChromeHistoryEmbeddingsService::PrepareQualityLogEntry() {
  if (!optimization_guide_service_) {
    return nullptr;
  }

  auto* quality_uploader =
      optimization_guide_service_->GetModelQualityLogsUploaderService();
  if (!quality_uploader) {
    return nullptr;
  }

  QualityLogEntry log_entry =
      std::make_unique<optimization_guide::ModelQualityLogEntry>(
          std::make_unique<optimization_guide::proto::LogAiDataRequest>(),
          quality_uploader->GetWeakPtr());

  optimization_guide::proto::LogAiDataRequest* request =
      log_entry->log_ai_data_request();
  if (!request) {
    return nullptr;
  }

  return log_entry;
}

}  // namespace history_embeddings
