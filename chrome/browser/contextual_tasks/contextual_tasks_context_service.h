// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

class OptimizationGuideKeyedService;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace passage_embeddings {
class Embedder;
class EmbedderMetadataProvider;
class PageEmbeddingsService;
}  // namespace passage_embeddings

namespace contextual_tasks {

enum class ContextDeterminationStatus {
  kSuccess = 0,
  kEmbedderNotAvailable = 1,
  kQueryEmbeddingFailed = 2,
  kQueryEmbeddingOutputMalformed = 3,

  // Keep in sync with ContextualTasksContextDeterminationStatus in
  // contextual_tasks/enums.xml.
  kMaxValue = kQueryEmbeddingOutputMalformed,
};

// A service used to determine the relevant context for a given task.
class ContextualTasksContextService
    : public KeyedService,
      public passage_embeddings::EmbedderMetadataObserver {
 public:
  ContextualTasksContextService(
      Profile* profile,
      passage_embeddings::PageEmbeddingsService* page_embeddings_service,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder,
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ContextualTasksContextService(const ContextualTasksContextService&) = delete;
  ContextualTasksContextService operator=(
      const ContextualTasksContextService&) = delete;
  ~ContextualTasksContextService() override;

  // Returns the relevant tabs for `query`. Will invoke `callback` when done.
  void GetRelevantTabsForQuery(
      const std::string& query,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback);

 private:
  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

  // Callback invoked when the embedding for `query` is ready.
  void OnQueryEmbeddingReady(
      const std::string& query,
      base::TimeTicks start_time,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Whether the embedder is available.
  bool is_embedder_available_ = false;

  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<Profile> profile_;
  raw_ptr<passage_embeddings::PageEmbeddingsService> page_embeddings_service_;
  raw_ptr<passage_embeddings::EmbedderMetadataProvider>
      embedder_metadata_provider_;
  raw_ptr<passage_embeddings::Embedder> embedder_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      scoped_observation_{this};

  base::WeakPtrFactory<ContextualTasksContextService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_
