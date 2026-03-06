// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_
#define CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/history_embeddings/content/history_embeddings_service.h"

class Profile;

namespace history {
class HistoryService;
}

namespace optimization_guide {
class OptimizationGuideDecider;
}

namespace page_content_annotations {
class PageContentAnnotationsService;
}

namespace passage_embeddings {
class Embedder;
class EmbedderMetadataProvider;
}  // namespace passage_embeddings

namespace history_embeddings {

// Specialization of HistoryEmbeddingsService with implementation details
// that only apply in Chrome, for example model quality logging.
class ChromeHistoryEmbeddingsService : public HistoryEmbeddingsService {
 public:
  ChromeHistoryEmbeddingsService(
      Profile* profile,
      history::HistoryService* history_service,
      page_content_annotations::PageContentAnnotationsService*
          page_content_annotations_service,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider,
      page_content_annotations::PageEmbeddingsService* page_embeddings_service,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder,
      std::unique_ptr<Answerer> answerer,
      std::unique_ptr<IntentClassifier> intent_classifier);
  explicit ChromeHistoryEmbeddingsService(const HistoryEmbeddingsService&) =
      delete;
  ChromeHistoryEmbeddingsService& operator=(const HistoryEmbeddingsService&) =
      delete;
  ~ChromeHistoryEmbeddingsService() override;

  // HistoryEmbeddingsService:
  bool IsAnswererUseAllowed() const override;

 private:
  QualityLogEntry PrepareQualityLogEntry() override;

  // Owns `this`. ChromeHistoryEmbeddingsService is a profile-keyed service.
  raw_ptr<Profile> profile_;
};

}  // namespace history_embeddings

#endif  // CHROME_BROWSER_HISTORY_EMBEDDINGS_CHROME_HISTORY_EMBEDDINGS_SERVICE_H_
