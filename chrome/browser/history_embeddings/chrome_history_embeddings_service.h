// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/history_embeddings/history_embeddings_service.h"
#include "content/public/browser/browser_context.h"

namespace history_embeddings {

// Specialization of HistoryEmbeddingsService with implementation details
// that only apply in Chrome, for example model quality logging.
class ChromeHistoryEmbeddingsService : public HistoryEmbeddingsService {
 public:
  ChromeHistoryEmbeddingsService(
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      OptimizationGuideKeyedService* optimization_guide_service,
      std::unique_ptr<Embedder> embedder,
      std::unique_ptr<Answerer> answerer,
      std::unique_ptr<IntentClassifier> intent_classifier);
  explicit ChromeHistoryEmbeddingsService(const HistoryEmbeddingsService&) =
      delete;
  ChromeHistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) =
      delete;
  ~ChromeHistoryEmbeddingsService() override;

 private:
  // HistoryEmbeddingsService:
  bool IsAnswererUseAllowed() const override;
  QualityLogEntry PrepareQualityLogEntry() override;

  // Outlives this because of service factory dependency.
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_service_;
};

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_
