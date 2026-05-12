// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_signal_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_tab_visit_tracker.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/site_exclusion_detail.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/prefs.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/features/contextual_tasks_context.pb.h"
#include "components/page_content_annotations/content/page_content_annotations_web_contents_observer.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/content/page_embeddings_service.h"
#include "components/page_content_annotations/core/page_content_extraction_types.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"

namespace contextual_tasks {

ContextualTasksContextService::QueryState::QueryState(
    std::string query,
    passage_embeddings::Embedding query_embedding,
    int query_word_count)
    : query(std::move(query)),
      query_embedding(std::move(query_embedding)),
      query_word_count(query_word_count) {}
ContextualTasksContextService::QueryState::~QueryState() = default;
ContextualTasksContextService::QueryState::QueryState(const QueryState&) =
    default;
ContextualTasksContextService::QueryState&
ContextualTasksContextService::QueryState::operator=(const QueryState&) =
    default;

namespace {

// Convenience macro for emitting OPTIMIZATION_GUIDE_LOGs where
// optimization_keyed_service_ is defined.
#define AUTO_CONTEXT_LOG(message)                                            \
  OPTIMIZATION_GUIDE_LOG(                                                    \
      optimization_guide_common::mojom::LogSource::CONTEXTUAL_TASKS_CONTEXT, \
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),       \
      (message))

std::optional<TabSimilarityScores> GetEmbeddingScores(
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<page_content_annotations::PassageEmbedding>&
        web_contents_embeddings) {
  if (web_contents_embeddings.empty()) {
    return std::nullopt;
  }

  TabSimilarityScores similarity_scores;
  for (const auto& embedding : web_contents_embeddings) {
    if (kOnlyUseTitlesForSimilarity.Get() &&
        embedding.passage.second !=
            page_content_annotations::EmbeddingPassageType::kTitle) {
      continue;
    }
    float similarity_score = embedding.embedding.ScoreWith(query_embedding);
    if (similarity_score > similarity_scores.best.score) {
      similarity_scores.best = {similarity_score, embedding.passage.first};
    }
    if (similarity_score < similarity_scores.worst.score) {
      similarity_scores.worst = {similarity_score, embedding.passage.first};
    }
  }
  return similarity_scores;
}

void RecordContextDeterminationStatus(ContextDeterminationStatus status) {
  base::UmaHistogramEnumeration(
      "ContextualTasks.Context.ContextDeterminationStatus", status);
}

void RecordTabSelectionMetrics(std::set<GURL> relevant_tab_urls,
                               std::set<GURL> explicit_urls) {
  CHECK(!explicit_urls.empty());

  std::set<GURL> explicit_url_set =
      std::set<GURL>(explicit_urls.begin(), explicit_urls.end());
  base::UmaHistogramCounts100("ContextualTasks.Context.ExplicitTabsCount",
                              explicit_url_set.size());

  // Calculate number/percentage of tabs that were predicted correctly based on
  // explicitly chosen set.
  base::flat_set<GURL> mutual_urls;
  std::set_intersection(relevant_tab_urls.begin(), relevant_tab_urls.end(),
                        explicit_url_set.begin(), explicit_url_set.end(),
                        std::inserter(mutual_urls, mutual_urls.end()));
  base::UmaHistogramCounts100("ContextualTasks.Context.TabOverlapCount",
                              mutual_urls.size());
  base::UmaHistogramPercentage(
      "ContextualTasks.Context.TabOverlapPercentage",
      explicit_urls.empty() ? 0
                            : 100 * mutual_urls.size() / explicit_urls.size());

  // Calculate number of tabs that were predicted incorrectly.
  base::flat_set<GURL> excess_urls;
  std::set_difference(relevant_tab_urls.begin(), relevant_tab_urls.end(),
                      explicit_url_set.begin(), explicit_url_set.end(),
                      std::inserter(excess_urls, excess_urls.end()));
  base::UmaHistogramCounts100("ContextualTasks.Context.TabExcessCount",
                              excess_urls.size());
}

void RecordCandidateTabMetrics(const TabSignals& tab_signals, double score) {
  // Signals.
  if (tab_signals.embedding_score.has_value()) {
    base::UmaHistogramCounts100(
        "ContextualTasks.Context.EmbeddingSimilarityScore",
        static_cast<int>(100 * *(tab_signals.embedding_score)));
  }
  if (tab_signals.duration_since_last_active.has_value()) {
    base::UmaHistogramLongTimes(
        "ContextualTasks.Context.DurationSinceLastActive",
        *(tab_signals.duration_since_last_active));
  }

  base::UmaHistogramCounts100("ContextualTasks.Context.MatchingWordsCount",
                              tab_signals.num_query_title_matching_words);

  // Scores.
  base::UmaHistogramSparse("ContextualTasks.Context.TabScore",
                           static_cast<int>(100 * score));
}

void PopulateTabContext(
    const TabSignals& tab_signals,
    base::span<const GURL> explicit_urls,
    double score,
    optimization_guide::proto::ContextualTasksTabContext* tab_context) {
  if (tab_signals.embedding_score.has_value()) {
    tab_context->set_best_embedding_score(*tab_signals.embedding_score);
  }
  if (tab_signals.duration_since_last_active.has_value()) {
    tab_context->set_seconds_since_last_active(
        tab_signals.duration_since_last_active->InSeconds());
  }

  tab_context->set_number_of_common_words(
      tab_signals.num_query_title_matching_words);

  if (tab_signals.duration_of_last_visit.has_value()) {
    tab_context->set_seconds_of_last_visit(
        tab_signals.duration_of_last_visit->InSeconds());
  }

  tab_context->set_query_title_similarity(
      tab_signals.query_candidate_tab_title_similarity);

  for (const auto& scored_passage :
       tab_signals.query_candidate_tab_passage_similarities) {
    tab_context->add_query_passage_similarities(scored_passage.score);
  }

  tab_context->set_active_tab_title_similarity(
      tab_signals.active_title_candidate_title_similarity);

  tab_context->set_aggregate_tab_score(score);
  if (tab_signals.web_contents) {
    tab_context->set_was_explicitly_chosen(std::ranges::contains(
        explicit_urls, tab_signals.web_contents->GetLastCommittedURL()));
  }
}

void PopulateTabSelectionModeInLog(
    mojom::TabSelectionMode mode,
    optimization_guide::proto::ContextualTasksContextQuality* quality_log) {
  if (!quality_log) {
    return;
  }
  switch (mode) {
    case mojom::TabSelectionMode::kStaticSignalsOnly:
      quality_log->set_tab_selection_mode(
          optimization_guide::proto::TabSelectionMode::
              TAB_SELECTION_MODE_STATIC_SIGNALS);
      break;
    case mojom::TabSelectionMode::kMultiSignalScoring:
      quality_log->set_tab_selection_mode(
          optimization_guide::proto::TabSelectionMode::
              TAB_SELECTION_MODE_STATIC_AND_ENGAGEMENT_SIGNALS);
      break;
    case mojom::TabSelectionMode::kStaticSignalsMlModel:
      quality_log->set_tab_selection_mode(
          optimization_guide::proto::TabSelectionMode::
              TAB_SELECTION_MODE_STATIC_ML_MODEL);
      break;
    // Do not set any value as server side logs proto misses this mode.
    case mojom::TabSelectionMode::kEmbeddingsMatch:
    default:
      break;
  }
}

double GetTabScoreSync(const TabSelectionOptions& options,
                       const TabSignals& tab_signals) {
  switch (options.tab_selection_mode) {
    case mojom::TabSelectionMode::kStaticSignalsOnly:
      return GetScoreWithStaticSignals(tab_signals);
    case mojom::TabSelectionMode::kMultiSignalScoring:
      return GetScoreWithAllSignals(tab_signals);
    case mojom::TabSelectionMode::kEmbeddingsMatch:
      return tab_signals.embedding_score.value_or(0.0);
    case mojom::TabSelectionMode::kStaticSignalsMlModel: {
      return 0.0;
    }
  }
}

std::vector<ScoredPassage> GetQueryTabPassageSimilarities(
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<page_content_annotations::PassageEmbedding>&
        tab_embeddings) {
  std::vector<ScoredPassage> similarity_scores;
  for (const auto& embedding : tab_embeddings) {
    if (embedding.passage.second !=
        page_content_annotations::EmbeddingPassageType::kTitle) {
      similarity_scores.emplace_back(
          ScoredPassage{embedding.embedding.ScoreWith(query_embedding),
                        embedding.passage.first});
    }
  }
  return similarity_scores;
}

const passage_embeddings::Embedding* GetTitleEmbedding(
    const std::vector<page_content_annotations::PassageEmbedding>&
        tab_embeddings) {
  auto it = std::ranges::find_if(tab_embeddings, [](const auto& embedding) {
    return embedding.passage.second ==
           page_content_annotations::EmbeddingPassageType::kTitle;
  });
  return it != tab_embeddings.end() ? &it->embedding : nullptr;
}

std::string GetFormattedQueryString(const std::string& query) {
  std::string task = kQueryEmbeddingTask.Get();
  if (!task.empty()) {
    return absl::StrFormat("task: %s | query: %s", task, query);
  }
  return query;
}

}  // namespace

ContextualTasksContextService::ContextualTasksContextService(Profile* profile)
    : profile_(profile),
      page_embeddings_service_(nullptr),
      embedder_metadata_provider_(nullptr),
      embedder_(nullptr),
      optimization_guide_keyed_service_(nullptr),
      page_content_extraction_service_(nullptr),
      tick_clock_(base::DefaultTickClock::GetInstance()) {}

ContextualTasksContextService::ContextualTasksContextService(
    Profile* profile,
    page_content_annotations::PageEmbeddingsService* page_embeddings_service,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder,
    OptimizationGuideKeyedService* optimization_guide_keyed_service,
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service)
    : profile_(profile),
      page_embeddings_service_(page_embeddings_service),
      embedder_metadata_provider_(embedder_metadata_provider),
      embedder_(embedder),
      optimization_guide_keyed_service_(optimization_guide_keyed_service),
      page_content_extraction_service_(page_content_extraction_service),
      tick_clock_(base::DefaultTickClock::GetInstance()) {
  scoped_embedder_metadata_provider_observation_.Observe(
      embedder_metadata_provider_);
  scoped_page_embeddings_service_observation_.Observe(page_embeddings_service_);

  if (optimization_guide_keyed_service_) {
    model_handler_ = std::make_unique<ContextualTasksContextModelHandler>(
        optimization_guide_keyed_service_,
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT}));
  }
}

ContextualTasksContextService::~ContextualTasksContextService() = default;

void ContextualTasksContextService::SetClockForTesting(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void ContextualTasksContextService::GetRelevantTabsForQuery(
    const TabSelectionOptions& options,
    const std::string& query,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        callback) {
  base::TimeTicks now = tick_clock_->NowTicks();

  AUTO_CONTEXT_LOG(base::StringPrintf("Processing query %s in mode %d", query,
                                      options.tab_selection_mode));

  if (!embedder_model_version_) {
    AUTO_CONTEXT_LOG("Embedder not available");
    RecordContextDeterminationStatus(
        ContextDeterminationStatus::kEmbedderNotAvailable);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       std::vector<base::WeakPtr<content::WebContents>>()));
    return;
  }

  AUTO_CONTEXT_LOG("Submitted query to embedder");
  int64_t request_id = next_request_id_++;

  if (options.tab_selection_timeout &&
      options.tab_selection_timeout->is_positive()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ContextualTasksContextService::OnRequestTimedOut,
                       weak_ptr_factory_.GetWeakPtr(), request_id),
        *options.tab_selection_timeout);
  }

  // TODO: crbug.com/452036470 - De-couple embeddings and recency signal
  // computation.
  passage_embeddings::Embedder::TaskId task_id =
      embedder_->ComputePassagesEmbeddings(
          passage_embeddings::PassagePriority::kUrgent,
          {GetFormattedQueryString(query)},
          base::BindOnce(&ContextualTasksContextService::OnQueryEmbeddingReady,
                         weak_ptr_factory_.GetWeakPtr(), query, options, now,
                         explicit_urls, request_id));
  pending_requests_[request_id] =
      std::make_unique<PendingRequest>(task_id, std::move(callback));
}

// TODO: crbug.com/503189770 - Integrate the multi-turn ML model. For now, just
// use the query from the current turn with the existing single-turn model.
void ContextualTasksContextService::GetRelevantTabsForConversationThread(
    const TabSelectionOptions& options,
    const ConversationThread& conversation_thread,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        callback) {
  GetRelevantTabsForQuery(options, conversation_thread.query, explicit_urls,
                          std::move(callback));
}

void ContextualTasksContextService::OnTypedQuery() {
  if (!embedder_model_version_) {
    // Do not queue if embedder is not available.
    return;
  }

  // Process embeddings for all tabs as the user has intent to call
  // `GetRelevantTabsForQuery` soon.
  page_embeddings_service_->ProcessEmbeddingsOnDemand();
}

void ContextualTasksContextService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  embedder_model_version_ = metadata.IsValid()
                                ? std::make_optional(metadata.model_version)
                                : std::nullopt;
}

page_content_annotations::PageEmbeddingsService::Priority
ContextualTasksContextService::GetDefaultPriority() const {
  return page_content_annotations::PageEmbeddingsService::Priority::kBackground;
}

void ContextualTasksContextService::OnQueryEmbeddingReady(
    const std::string& query,
    const TabSelectionOptions& options,
    base::TimeTicks start_time,
    const std::vector<GURL>& explicit_urls,
    int64_t request_id,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  base::UmaHistogramTimes("ContextualTasks.Context.QueryEmbeddingLatency",
                          tick_clock_->NowTicks() - start_time);

  auto request_it = pending_requests_.find(request_id);
  if (request_it == pending_requests_.end()) {
    // We had timed out already and the callback was already invoked.
    return;
  }

  base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
      callback = std::move(request_it->second->callback);
  pending_requests_.erase(request_id);

  // Query embedding was not successfully generated.
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    AUTO_CONTEXT_LOG(
        base::StringPrintf("Query embedding for %s failed", query));
    RecordContextDeterminationStatus(
        ContextDeterminationStatus::kQueryEmbeddingFailed);
    std::move(callback).Run({});
    return;
  }
  // Unexpected output size. Just return.
  if (embeddings.size() != 1u) {
    AUTO_CONTEXT_LOG(base::StringPrintf(
        "Query embedding for %s had unexpected output", query));
    RecordContextDeterminationStatus(
        ContextDeterminationStatus::kQueryEmbeddingOutputMalformed);
    std::move(callback).Run({});
    return;
  }

  AUTO_CONTEXT_LOG(
      base::StringPrintf("Processing query embedding for %s", query));

  std::vector<base::WeakPtr<content::WebContents>> all_tabs =
      GetAllEligibleTabs(options.browser_window_interface);
  if (all_tabs.empty()) {
    AUTO_CONTEXT_LOG("No eligible tabs");
    RecordContextDeterminationStatus(
        ContextDeterminationStatus::kNoEligibleTabs);
    std::move(callback).Run({});
    return;
  }

  RecordContextDeterminationStatus(ContextDeterminationStatus::kSuccess);

  auto log_entry = std::make_unique<optimization_guide::ModelQualityLogEntry>(
      optimization_guide_keyed_service_->GetModelQualityLogsUploaderService()
          ->GetWeakPtr());

  passage_embeddings::Embedding query_embedding = embeddings[0];
  auto* quality_log = log_entry->log_ai_data_request()
                          ->mutable_contextual_tasks_context()
                          ->mutable_quality();
  quality_log->set_embedding_model_version(
      embedder_model_version_.value_or(-1));

  SelectRelevantTabs(
      query, options, query_embedding, all_tabs, explicit_urls,
      base::BindOnce(&ContextualTasksContextService::OnRelevantTabsSelected,
                     weak_ptr_factory_.GetWeakPtr(), query, options, start_time,
                     explicit_urls, std::move(callback), std::move(log_entry)),
      quality_log);
}

void ContextualTasksContextService::OnRelevantTabsSelected(
    const std::string& query,
    const TabSelectionOptions& options,
    base::TimeTicks start_time,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        callback,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry,
    std::vector<base::WeakPtr<content::WebContents>> relevant_tabs) {
  AUTO_CONTEXT_LOG(base::StringPrintf(
      "Number of relevant tabs for query %s: %d", query, relevant_tabs.size()));

  base::UmaHistogramTimes("ContextualTasks.Context.ContextCalculationLatency",
                          tick_clock_->NowTicks() - start_time);
  base::UmaHistogramCounts100("ContextualTasks.Context.RelevantTabsCount",
                              relevant_tabs.size());

  if (!explicit_urls.empty()) {
    std::set<GURL> relevant_tab_url_set;
    for (const auto& web_contents : relevant_tabs) {
      if (web_contents) {
        relevant_tab_url_set.insert(web_contents->GetLastCommittedURL());
      }
    }
    RecordTabSelectionMetrics(
        relevant_tab_url_set,
        std::set<GURL>(explicit_urls.begin(), explicit_urls.end()));
  }

  if (!ShouldLogContextualTasksContextQuality() || !log_entry ||
      log_entry->log_ai_data_request()
              ->contextual_tasks_context()
              .quality()
              .eligible_tabs()
              .size() == 0) {
    // Explicitly drop when we don't want to log. Otherwise, the destructor of
    // the log entry will trigger an upload.
    optimization_guide::ModelQualityLogEntry::Drop(std::move(log_entry));
  }

  std::move(callback).Run(std::move(relevant_tabs));
}

void ContextualTasksContextService::OnRequestTimedOut(int64_t request_id) {
  auto request_it = pending_requests_.find(request_id);
  if (request_it == pending_requests_.end()) {
    // We had already completed the request and the callback was already
    // invoked.
    return;
  }

  passage_embeddings::Embedder::TaskId task_id = request_it->second->task_id;
  embedder_->TryCancel(task_id);
  base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
      callback = std::move(request_it->second->callback);
  pending_requests_.erase(request_id);
  RecordContextDeterminationStatus(ContextDeterminationStatus::kTimedOut);
  std::move(callback).Run({});
}

std::vector<base::WeakPtr<content::WebContents>>
ContextualTasksContextService::GetAllEligibleTabs(
    base::WeakPtr<BrowserWindowInterface> browser_window_interface) {
  std::vector<base::WeakPtr<content::WebContents>> all_tabs;
  SiteExclusionDetail site_exclusion_detail;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &all_tabs, browser_window_interface,
       &site_exclusion_detail](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile_) {
          return true;
        }
        if (browser_window_interface &&
            browser != browser_window_interface.get()) {
          return true;
        }
        TabListInterface* tab_list = TabListInterface::From(browser);
        CHECK(tab_list);
        for (int i = 0; i < tab_list->GetTabCount(); i++) {
          tabs::TabInterface* tab = tab_list->GetTab(i);
          content::WebContents* web_contents =
              tab ? tab->GetContents() : nullptr;
          if (!IsValidUrlForSuggestedTab(web_contents->GetLastCommittedURL(),
                                         profile_, site_exclusion_detail)) {
            AUTO_CONTEXT_LOG(
                base::StringPrintf("Removing %s from relevant set as it is not "
                                   "valid e.g. it is NTP, internal page, etc.",
                                   web_contents->GetLastCommittedURL().spec()));
            continue;
          }
          if (!ShouldAddTabToSelection(web_contents)) {
            AUTO_CONTEXT_LOG(
                base::StringPrintf("Removing %s from relevant set as it is not "
                                   "eligible for server upload",
                                   web_contents->GetLastCommittedURL().spec()));
            continue;
          }
          all_tabs.push_back(web_contents->GetWeakPtr());
        }
        // Stop iterating if the browser window interface is specified.
        return !browser_window_interface;
      });
  site_exclusion_detail.RecordAllTabsMetrics();
  return all_tabs;
}

content::WebContents* ContextualTasksContextService::GetActiveTabWebContents() {
  content::WebContents* active_tab_contents = nullptr;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, &active_tab_contents](BrowserWindowInterface* browser) {
        if (browser->GetProfile() == profile_) {
          if (auto* tab_list = TabListInterface::From(browser)) {
            if (auto* active_tab = tab_list->GetActiveTab()) {
              active_tab_contents = active_tab->GetContents();
            }
          }
          return false;
        }
        return true;
      });
  return active_tab_contents;
}

void ContextualTasksContextService::PopulateQueryContext(
    const QueryState& query_state,
    optimization_guide::proto::ContextualTasksContextQuality* quality_log) {
  quality_log->set_number_of_query_words(query_state.query_word_count);
  if (query_state.active_tab_title_similarity.has_value()) {
    quality_log->set_query_active_tab_title_similarity(
        *query_state.active_tab_title_similarity);
  }
  for (const auto& passage : query_state.active_tab_passage_similarities) {
    quality_log->add_query_active_tab_passage_similarities(passage.score);
  }
}

ContextualTasksContextService::QueryState
ContextualTasksContextService::CreateQueryState(
    const std::string& query,
    const passage_embeddings::Embedding& query_embedding) {
  QueryState query_state(query, query_embedding, GetWordCount(query));

  content::WebContents* active_tab_contents = GetActiveTabWebContents();
  SiteExclusionDetail site_exclusion_detail;
  if (IsValidUrlForSuggestedTab(active_tab_contents->GetLastCommittedURL(),
                                profile_, site_exclusion_detail)) {
    query_state.active_tab = active_tab_contents->GetWeakPtr();
    query_state.active_tab_embeddings = page_embeddings_service_->GetEmbeddings(
        active_tab_contents->GetPrimaryPage());

    const passage_embeddings::Embedding* active_tab_title_embedding =
        GetTitleEmbedding(query_state.active_tab_embeddings);
    if (active_tab_title_embedding) {
      query_state.active_tab_title_embedding = *active_tab_title_embedding;
      query_state.active_tab_title_similarity =
          query_embedding.ScoreWith(*active_tab_title_embedding);
    }

    query_state.active_tab_passage_similarities =
        GetQueryTabPassageSimilarities(query_embedding,
                                       query_state.active_tab_embeddings);
  }
  site_exclusion_detail.RecordActiveTabMetrics();

  return query_state;
}

TabSignals ContextualTasksContextService::ComputeTabSignals(
    content::WebContents* web_contents,
    const QueryState& query_state) {
  TabSignals tab_signals;
  tab_signals.web_contents = web_contents->GetWeakPtr();

  std::vector<page_content_annotations::PassageEmbedding>
      candidate_tab_embeddings;
  if (query_state.active_tab && query_state.active_tab.get() == web_contents) {
    candidate_tab_embeddings = query_state.active_tab_embeddings;
  } else {
    candidate_tab_embeddings =
        page_embeddings_service_->GetEmbeddings(web_contents->GetPrimaryPage());
  }

  tab_signals.similarity_scores =
      GetEmbeddingScores(query_state.query_embedding, candidate_tab_embeddings);

  if (tab_signals.similarity_scores) {
    tab_signals.embedding_score = tab_signals.similarity_scores->best.score;
  }

  tab_signals.num_query_title_matching_words = GetMatchingWordsCount(
      query_state.query, base::UTF16ToUTF8(web_contents->GetTitle()));

  const passage_embeddings::Embedding* candidate_tab_title_embedding =
      GetTitleEmbedding(candidate_tab_embeddings);
  if (candidate_tab_title_embedding) {
    tab_signals.query_candidate_tab_title_similarity =
        query_state.query_embedding.ScoreWith(*candidate_tab_title_embedding);
    if (query_state.active_tab_title_embedding) {
      tab_signals.active_title_candidate_title_similarity =
          query_state.active_tab_title_embedding->ScoreWith(
              *candidate_tab_title_embedding);
    }
  }
  tab_signals.query_candidate_tab_passage_similarities =
      GetQueryTabPassageSimilarities(query_state.query_embedding,
                                     candidate_tab_embeddings);

  tab_signals.duration_since_last_active =
      GetDurationSinceLastActive(web_contents);
  tab_signals.duration_of_last_visit =
      GetDurationOfCurrentOrLastVisit(web_contents);

  return tab_signals;
}

ContextualTasksContextService::ScoringState::ScoringState(size_t size)
    : scores(size, 0.0) {
  signals.reserve(size);
  for (size_t k = 0; k < size; ++k) {
    signals.emplace_back();
  }
}

ContextualTasksContextService::ScoringState::~ScoringState() = default;

void ContextualTasksContextService::SelectRelevantTabs(
    const std::string& query,
    const TabSelectionOptions& options,
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<base::WeakPtr<content::WebContents>>& all_tabs,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        on_tab_selection_complete,
    optimization_guide::proto::ContextualTasksContextQuality* quality_log) {
  PopulateTabSelectionModeInLog(options.tab_selection_mode, quality_log);
  QueryState query_state = CreateQueryState(query, query_embedding);
  PopulateQueryContext(query_state, quality_log);

  QueryStateSignals query_signals;
  query_signals.query_word_count = query_state.query_word_count;
  query_signals.query_active_tab_title_similarity =
      query_state.active_tab_title_similarity.value_or(0.0f);
  query_signals.query_active_tab_passage_similarities =
      query_state.active_tab_passage_similarities;

  AUTO_CONTEXT_LOG(base::StringPrintf(
      "Number of eligible open tabs for query %s: %d", query, all_tabs.size()));

  auto scoring_state = base::MakeRefCounted<ScoringState>(all_tabs.size());

  for (size_t i = 0; i < all_tabs.size(); ++i) {
    const auto& web_contents = all_tabs[i];
    if (!web_contents) {
      continue;
    }
    scoring_state->signals[i] =
        ComputeTabSignals(web_contents.get(), query_state);
  }

  if (options.tab_selection_mode ==
          mojom::TabSelectionMode::kStaticSignalsMlModel &&
      model_handler_) {
    model_handler_->BatchExecuteModelWithSignals(
        query_signals, scoring_state->signals,
        base::BindOnce(
            [](base::OnceClosure done_callback,
               scoped_refptr<ScoringState> scoring_state,
               const std::vector<std::optional<float>>& scores) {
              std::ranges::transform(
                  scores, scoring_state->scores.begin(),
                  [](const std::optional<float>& score) {
                    return static_cast<double>(score.value_or(0.0f));
                  });
              std::move(done_callback).Run();
            },
            base::BindOnce(&ContextualTasksContextService::OnAllTabsScored,
                           weak_ptr_factory_.GetWeakPtr(), query, options,
                           all_tabs, explicit_urls,
                           std::move(on_tab_selection_complete), scoring_state,
                           quality_log),
            scoring_state));
    return;
  }

  for (size_t i = 0; i < all_tabs.size(); ++i) {
    if (!all_tabs[i]) {
      continue;
    }
    scoring_state->scores[i] =
        GetTabScoreSync(options, scoring_state->signals[i]);
  }

  OnAllTabsScored(query, options, all_tabs, explicit_urls,
                  std::move(on_tab_selection_complete), scoring_state,
                  quality_log);
}

void ContextualTasksContextService::OnAllTabsScored(
    const std::string& query,
    const TabSelectionOptions& options,
    const std::vector<base::WeakPtr<content::WebContents>>& all_tabs,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        on_tab_selection_complete,
    scoped_refptr<ScoringState> scoring_state,
    optimization_guide::proto::ContextualTasksContextQuality* quality_log) {
  std::vector<std::pair<double, base::WeakPtr<content::WebContents>>>
      scored_relevant_tabs;

  for (size_t i = 0; i < all_tabs.size(); ++i) {
    const auto& web_contents = all_tabs[i];
    if (!web_contents) {
      continue;
    }

    double score = scoring_state->scores[i];
    const TabSignals& tab_signals = scoring_state->signals[i];

    if (score >=
        options.min_model_score.value_or(kTabSelectionScoreThreshold.Get())) {
      scored_relevant_tabs.emplace_back(score, web_contents);
    }

    // Recording signals and scores for analysis.
    RecordCandidateTabMetrics(tab_signals, score);
    optimization_guide::proto::ContextualTasksTabContext* tab_context =
        quality_log->add_eligible_tabs();
    PopulateTabContext(tab_signals, explicit_urls, score, tab_context);

    // Print debug logs.
    if (tab_signals.similarity_scores) {
      AUTO_CONTEXT_LOG(
          absl::StrFormat("Passage with highest similarity with query %s: %f",
                          tab_signals.similarity_scores->best.text,
                          tab_signals.similarity_scores->best.score));
      AUTO_CONTEXT_LOG(
          absl::StrFormat("Passage with lowest similarity with query %s: %f",
                          tab_signals.similarity_scores->worst.text,
                          tab_signals.similarity_scores->worst.score));
    }

    AUTO_CONTEXT_LOG(absl::StrFormat(
        "Query: %s | TabTitle: %s | Score: %f\n"
        "  EmbeddingsScore: %f\n"
        "  SecondsSinceLastActive: %.3f\n"
        "  MatchingWordsCount: %d\n"
        "  DurationOfLastVisitInSeconds: %.3f",
        query,
        (tab_signals.web_contents
             ? base::UTF16ToUTF8(tab_signals.web_contents->GetTitle())
             : ""),
        score, tab_signals.embedding_score.value_or(0.0f),
        tab_signals.duration_since_last_active.has_value()
            ? tab_signals.duration_since_last_active->InSecondsF()
            : -1.0,
        tab_signals.num_query_title_matching_words,
        tab_signals.duration_of_last_visit.has_value()
            ? tab_signals.duration_of_last_visit->InSecondsF()
            : -1.0));
  }

  std::sort(scored_relevant_tabs.begin(), scored_relevant_tabs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });

  std::vector<base::WeakPtr<content::WebContents>> relevant_tabs;
  base::flat_set<GURL> seen_urls;
  std::ranges::for_each(
      scored_relevant_tabs, [&](const auto& score_and_contents) {
        if (score_and_contents.second &&
            seen_urls.insert(score_and_contents.second->GetLastCommittedURL())
                .second) {
          relevant_tabs.push_back(score_and_contents.second);
        }
      });

  std::move(on_tab_selection_complete).Run(std::move(relevant_tabs));
}

std::optional<base::TimeDelta>
ContextualTasksContextService::GetDurationSinceLastActive(
    content::WebContents* web_contents) {
  if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
    if (auto* tracker = ContextualTasksTabVisitTracker::From(tab)) {
      return tracker->GetDurationSinceLastActive();
    }
  }
  return std::nullopt;
}

std::optional<base::TimeDelta>
ContextualTasksContextService::GetDurationOfCurrentOrLastVisit(
    content::WebContents* web_contents) {
  if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
    if (auto* tracker = ContextualTasksTabVisitTracker::From(tab)) {
      return tracker->GetDurationOfCurrentOrLastVisit();
    }
  }
  return std::nullopt;
}

bool ContextualTasksContextService::ShouldAddTabToSelection(
    content::WebContents* web_contents) {
  // Get sensitivity.
  bool is_sensitive = false;
  if (auto* page_content_annotations_observer =
          page_content_annotations::PageContentAnnotationsWebContentsObserver::
              FromWebContents(web_contents)) {
    float visibility_score =
        page_content_annotations_observer->content_visibility_score().value_or(
            -1.0f);
    is_sensitive = visibility_score < kContentVisibilityThreshold.Get() &&
                   visibility_score >= 0.0;
  }

  // Get whether it's eligible for server upload.
  bool is_eligible_for_server_upload = true;
  if (page_content_extraction_service_) {
    is_eligible_for_server_upload =
        page_content_extraction_service_
            ->GetServerUploadEligibilityForPage(web_contents->GetPrimaryPage())
            .value_or(true);
  }

  return is_eligible_for_server_upload && !is_sensitive;
}

ContextualTasksContextService::PendingRequest::PendingRequest(
    passage_embeddings::Embedder::TaskId task_id,
    base::OnceCallback<void(std::vector<base::WeakPtr<content::WebContents>>)>
        callback)
    : task_id(task_id), callback(std::move(callback)) {}
ContextualTasksContextService::PendingRequest::~PendingRequest() = default;

TabSelectionOptions::TabSelectionOptions() = default;
TabSelectionOptions::~TabSelectionOptions() = default;
TabSelectionOptions::TabSelectionOptions(const TabSelectionOptions&) = default;

ThreadTurn::ThreadTurn() = default;
ThreadTurn::~ThreadTurn() = default;
ThreadTurn::ThreadTurn(const ThreadTurn&) = default;
ThreadTurn& ThreadTurn::operator=(const ThreadTurn&) = default;

ConversationThread::ConversationThread() = default;
ConversationThread::~ConversationThread() = default;
ConversationThread::ConversationThread(const ConversationThread&) = default;
ConversationThread& ConversationThread::operator=(const ConversationThread&) = default;

}  // namespace contextual_tasks
