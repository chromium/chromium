// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class GURL;
class OptimizationGuideKeyedService;
class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide::proto {
class ContextualTasksContextQuality;
}  // namespace optimization_guide::proto

namespace page_content_annotations {
class PageContentExtractionService;
class PageEmbeddingsService;
}  // namespace page_content_annotations

namespace passage_embeddings {
class Embedder;
class EmbedderMetadataProvider;
}  // namespace passage_embeddings

namespace contextual_tasks {

enum class ContextDeterminationStatus {
  kSuccess = 0,
  kEmbedderNotAvailable = 1,
  kQueryEmbeddingFailed = 2,
  kQueryEmbeddingOutputMalformed = 3,
  kNoEligibleTabs = 4,
  kTimedOut = 5,

  // Keep in sync with ContextualTasksContextDeterminationStatus in
  // contextual_tasks/enums.xml.
  kMaxValue = kTimedOut,
};

// Options to regulate tab selection behavior.
struct TabSelectionOptions {
  mojom::TabSelectionMode tab_selection_mode =
      mojom::TabSelectionMode::kMultiSignalScoring;

  // If set, only tabs with a model score of at least `min_model_score` will be
  // selected.
  std::optional<float> min_model_score;

  // If set, tab selection will time out after this duration.
  std::optional<base::TimeDelta> tab_selection_timeout;
};

// A service used to determine the relevant context for a given task.
class ContextualTasksContextService
    : public KeyedService,
      public passage_embeddings::EmbedderMetadataObserver,
      public page_content_annotations::PageEmbeddingsService::Observer {
 public:
  ContextualTasksContextService(
      Profile* profile,
      page_content_annotations::PageEmbeddingsService* page_embeddings_service,
      passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
      passage_embeddings::Embedder* embedder,
      OptimizationGuideKeyedService* optimization_guide_keyed_service,
      page_content_annotations::PageContentExtractionService*
          page_content_extraction_service);
  ContextualTasksContextService(const ContextualTasksContextService&) = delete;
  ContextualTasksContextService operator=(
      const ContextualTasksContextService&) = delete;
  ~ContextualTasksContextService() override;

  // Returns the relevant tabs for `query`. Will invoke `callback` when done.
  void GetRelevantTabsForQuery(
      const TabSelectionOptions& options,
      const std::string& query,
      const std::vector<GURL>& explicit_urls,
      base::OnceCallback<void(std::vector<content::WebContents*>)> callback);

  void SetClockForTesting(const base::TickClock* tick_clock);

 private:
  // EmbedderMetadataObserver:
  void EmbedderMetadataUpdated(
      passage_embeddings::EmbedderMetadata metadata) override;

  // page_content_annotations::PageEmbeddingsService::Observer:
  page_content_annotations::PageEmbeddingsService::Priority GetDefaultPriority()
      const override;

  // Callback invoked when the embedding for `query` is ready.
  void OnQueryEmbeddingReady(
      const std::string& query,
      const TabSelectionOptions& options,
      base::TimeTicks start_time,
      const std::vector<GURL>& explicit_urls,
      int64_t request_id,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Callback invoked when the request has timed out.
  void OnRequestTimedOut(int64_t request_id);

  // Returns all tabs for the profile that are eligible for selection.
  std::vector<content::WebContents*> GetAllEligibleTabs();

  // Returns the relevant tabs for `query`. Collects and logs all the signals
  // irrespective of chosen `tab_selection_mode`.
  std::vector<content::WebContents*> SelectRelevantTabs(
      const std::string& query,
      const TabSelectionOptions& options,
      const passage_embeddings::Embedding& query_embedding,
      const std::vector<content::WebContents*>& all_tabs,
      const std::vector<GURL>& explicit_urls,
      optimization_guide::proto::ContextualTasksContextQuality* quality_log);

  // Returns the duration since the tab was last active.
  std::optional<base::TimeDelta> GetDurationSinceLastActive(
      content::WebContents* web_contents);

  // Returns the time spent in the tab on its last visit. If the tab is still
  // active, then it returns the time spent in the current visit.
  std::optional<base::TimeDelta> GetDurationOfCurrentOrLastVisit(
      content::WebContents* web_contents);

  // Returns whether the tab should be added to the selection.
  bool ShouldAddTabToSelection(content::WebContents* web_contents);

  // The version of the embedder model.
  std::optional<int64_t> embedder_model_version_;

  // Not owned. Guaranteed to outlive `this`.
  raw_ptr<Profile> profile_;
  raw_ptr<page_content_annotations::PageEmbeddingsService>
      page_embeddings_service_;
  raw_ptr<passage_embeddings::EmbedderMetadataProvider>
      embedder_metadata_provider_;
  raw_ptr<passage_embeddings::Embedder> embedder_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
  raw_ptr<page_content_annotations::PageContentExtractionService>
      page_content_extraction_service_;
  raw_ptr<const base::TickClock> tick_clock_;

  struct PendingRequest {
    PendingRequest(
        passage_embeddings::Embedder::TaskId task_id,
        base::OnceCallback<void(std::vector<content::WebContents*>)> callback);
    ~PendingRequest();

    passage_embeddings::Embedder::TaskId task_id;
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback;
  };
  absl::flat_hash_map<int64_t, std::unique_ptr<PendingRequest>>
      pending_requests_;
  int64_t next_request_id_ = 0;

  base::ScopedObservation<passage_embeddings::EmbedderMetadataProvider,
                          passage_embeddings::EmbedderMetadataObserver>
      scoped_embedder_metadata_provider_observation_{this};
  base::ScopedObservation<
      page_content_annotations::PageEmbeddingsService,
      page_content_annotations::PageEmbeddingsService::Observer>
      scoped_page_embeddings_service_observation_{this};

  base::WeakPtrFactory<ContextualTasksContextService> weak_ptr_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_
