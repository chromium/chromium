// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_types.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class BrowserWindowInterface;
class GURL;
class OptimizationGuideKeyedService;
class Profile;

namespace optimization_guide {
class ModelQualityLogEntry;
}  // namespace optimization_guide

namespace content {
class WebContents;
}  // namespace content

namespace optimization_guide::proto {
class ContextualTasksContextQuality;
class ContextualTasksTabContext;
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

struct SiteExclusionDetail;
class ContextualTasksContextModelHandler;
struct ConversationThread;

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
      mojom::TabSelectionMode::kStaticSignalsMlModel;

  // If set, only tabs with a model score of at least `min_model_score` will be
  // selected.
  std::optional<float> min_model_score;

  // If set, tab selection will time out after this duration.
  std::optional<base::TimeDelta> tab_selection_timeout;

  // If non-null, only tabs from this browser window will be selected.
  base::WeakPtr<BrowserWindowInterface> browser_window_interface;

  TabSelectionOptions();
  ~TabSelectionOptions();
  TabSelectionOptions(const TabSelectionOptions&);
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

  // Returns whether smart tab sharing is enabled for `profile`.
  static bool GetIsSmartTabSharingEnabled(const Profile* profile);

  // Returns the relevant tabs for `query`. Will invoke `callback` when done.
  virtual void GetRelevantTabsForQuery(
      const TabSelectionOptions& options,
      const std::string& query,
      const std::vector<GURL>& explicit_urls,
      base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
          callback);

  // Returns the relevant tabs for `conversation_thread`.
  virtual void GetRelevantTabsForConversationThread(
      const TabSelectionOptions& options,
      const ConversationThread& conversation_thread,
      const std::vector<GURL>& explicit_urls,
      base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
          callback);

  // Called when the user starts typing a query.
  //
  // This will pre-flight any pending embeddings required.
  void OnTypedQuery();

  void SetClockForTesting(const base::TickClock* tick_clock);

 protected:
  // Constructor for testing that avoids initializing other dependencies.
  explicit ContextualTasksContextService(Profile* profile);

 private:
  friend class ContextualTasksContextServiceTest;

  struct QueryState {
    QueryState(std::string query,
               passage_embeddings::Embedding query_embedding,
               int query_word_count);
    ~QueryState();
    QueryState(const QueryState&);
    QueryState& operator=(const QueryState&);

    std::string query;
    passage_embeddings::Embedding query_embedding;
    int query_word_count = 0;

    // Using this tab to contextualize the query e.g. this will be the active
    // tab when the query comes from the side panel, or the most recent tab if
    // the query comes from the AIM full tab or NTP.
    base::WeakPtr<content::WebContents> context_tab;
    std::vector<page_content_annotations::PassageEmbedding>
        context_tab_passage_embeddings;

    std::optional<passage_embeddings::Embedding> context_tab_title_embedding;
    std::optional<float> context_tab_title_similarity;
    std::vector<ScoredPassage> context_tab_passage_similarities;
  };

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
      std::optional<base::WeakPtr<content::WebContents>>
          active_tab_at_query_time,
      const std::vector<GURL>& explicit_urls,
      int64_t request_id,
      std::vector<std::string> passages,
      std::vector<passage_embeddings::Embedding> embeddings,
      passage_embeddings::Embedder::TaskId task_id,
      passage_embeddings::ComputeEmbeddingsStatus status);

  // Callback invoked when the request has timed out.
  void OnRequestTimedOut(int64_t request_id);

  // Callback invoked when relevant tabs are selected.
  void OnRelevantTabsSelected(
      const std::string& query,
      const TabSelectionOptions& options,
      base::TimeTicks start_time,
      const std::vector<GURL>& explicit_urls,
      int64_t request_id,
      std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
      std::vector<base::WeakPtr<content::WebContents>> relevant_tabs);

  // Intermediate state for asynchronous scoring.
  struct ScoringState : public base::RefCounted<ScoringState> {
    explicit ScoringState(size_t size);

    std::vector<double> scores;
    std::vector<TabSignals> signals;

   private:
    friend class base::RefCounted<ScoringState>;
    ~ScoringState();
  };

  // Callback invoked when all open tabs have been scored.
  void OnAllTabsScored(
      const std::string& query,
      const TabSelectionOptions& options,
      const std::vector<base::WeakPtr<content::WebContents>>& all_eligible_tabs,
      const std::vector<GURL>& explicit_urls,
      base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
          on_selection_complete,
      scoped_refptr<ScoringState> scoring_state,
      optimization_guide::proto::ContextualTasksContextQuality* quality_log);

  // Returns all tabs for the profile that are eligible for selection.
  //
  // This function will scope the eligible tabs to what's in
  // `browser_window_interface` if it is not null.
  std::vector<base::WeakPtr<content::WebContents>> GetAllEligibleTabs(
      base::WeakPtr<BrowserWindowInterface> browser_window_interface);

  // Returns the tab that should be used to contextualize the query.
  content::WebContents* GetQueryContextualizingTab(
      const std::vector<base::WeakPtr<content::WebContents>>& all_eligible_tabs,
      std::optional<base::WeakPtr<content::WebContents>>
          active_tab_at_query_time,
      SiteExclusionDetail& site_exclusion_detail);

  // Creates the QueryState including active tab context.
  QueryState CreateQueryState(
      const std::string& query,
      const passage_embeddings::Embedding& query_embedding,
      std::optional<base::WeakPtr<content::WebContents>>
          active_tab_at_query_time,
      const std::vector<base::WeakPtr<content::WebContents>>&
          all_eligible_tabs);

  // Computes TabSignals for a candidate tab.
  TabSignals ComputeTabSignals(content::WebContents* web_contents,
                               const QueryState& query_state);

  // Returns the relevant tabs for `query`. Collects and logs all the signals
  // irrespective of chosen `tab_selection_mode`.
  void SelectRelevantTabs(
      const std::string& query,
      const TabSelectionOptions& options,
      const passage_embeddings::Embedding& query_embedding,
      std::optional<base::WeakPtr<content::WebContents>>
          active_tab_at_query_time,
      const std::vector<base::WeakPtr<content::WebContents>>& all_eligible_tabs,
      const std::vector<GURL>& explicit_urls,
      base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
          on_selection_complete,
      optimization_guide::proto::ContextualTasksContextQuality* quality_log);

  // Helper method to populate query state context. These are common for all
  // candidate tabs.
  void PopulateQueryContext(
      const QueryState& query_state,
      optimization_guide::proto::ContextualTasksContextQuality* quality_log);

  // Returns the WebContents of the currently active tab.
  content::WebContents* GetActiveTabWebContents();

  // Returns whether the tab is valid i.e. it is not NTP, internal page, etc.
  // `site_exclusion_detail` is updated with results of site exclusion filtering
  // for metrics.
  bool IsValidTab(content::WebContents* web_contents,
                  SiteExclusionDetail& site_exclusion_detail);

  // Returns whether the tab should be added to the selection.
  bool ShouldAddTabToSelection(content::WebContents* web_contents);

  std::unique_ptr<ContextualTasksContextModelHandler> model_handler_;

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
        passage_embeddings::Embedder::Job job,
        base::OnceCallback<
            void(std::vector<base::WeakPtr<content::WebContents>>)> callback);
    ~PendingRequest();

    passage_embeddings::Embedder::Job job;
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        callback;
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
