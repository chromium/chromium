// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_signal_utils.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_types.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

// Convenience macro for emitting OPTIMIZATION_GUIDE_LOGs where
// optimization_keyed_service_ is defined.
#define AUTO_CONTEXT_LOG(message)                                            \
  OPTIMIZATION_GUIDE_LOG(                                                    \
      optimization_guide_common::mojom::LogSource::CONTEXTUAL_TASKS_CONTEXT, \
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),       \
      (message))

struct TabSignals {
  raw_ptr<content::WebContents> web_contents = nullptr;
  std::optional<float> embedding_score;
  std::optional<base::TimeDelta> duration_since_last_active;
  std::optional<int> num_query_title_matching_words;
};

std::optional<float> GetBestEmbeddingScore(
    content::WebContents* web_contents,
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<passage_embeddings::PassageEmbedding>&
        web_contents_embeddings) {
  float best_similarity_score = 0.0f;
  if (web_contents_embeddings.empty()) {
    return std::nullopt;
  }

  for (const auto& embedding : web_contents_embeddings) {
    if (kOnlyUseTitlesForSimilarity.Get() &&
        embedding.passage.second != passage_embeddings::PassageType::kTitle) {
      continue;
    }
    float similarity_score = embedding.embedding.ScoreWith(query_embedding);
    if (similarity_score > best_similarity_score) {
      best_similarity_score = similarity_score;
    }
  }
  return best_similarity_score;
}

// Probabilistic OR - any high score leads to high score.
float ProbOr(const float score1, const float score2) {
  return 1.0f - (1.0f - score1) * (1.0f - score2);
}

// TODO: crbug.com/452036470 - Add a proper scoring function based on analysis.
double GetTabScore(const TabSignals& signals) {
  double score = 0;
  if (signals.embedding_score.has_value()) {
    score = ProbOr(score, *(signals.embedding_score));
  }
  if (signals.duration_since_last_active.has_value()) {
    score = ProbOr(
        score,
        std::pow(0.7, signals.duration_since_last_active->InSeconds() / 180));
  }
  if (signals.num_query_title_matching_words.has_value()) {
    // Monotonically increasing; Always < 1.
    // 0 matches = 0 score; 1 match = 0.57; 2 matches = 0.81 and so on.
    float lexical_match_score =
        1.0f - std::exp(-0.85 * *(signals.num_query_title_matching_words));
    score = ProbOr(score, lexical_match_score);
  }
  return score;
}

std::vector<content::WebContents*> GetAllTabsForProfile(Profile* profile) {
  std::vector<content::WebContents*> all_tabs;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [profile, &all_tabs](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }
        TabStripModel* const tab_strip_model = browser->GetTabStripModel();
        for (int i = 0; i < tab_strip_model->count(); i++) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(i);
          if (web_contents &&
              web_contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
            all_tabs.push_back(web_contents);
          }
        }
        return true;
      });
  return all_tabs;
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

}  // namespace

ContextualTasksContextService::ContextualTasksContextService(
    Profile* profile,
    passage_embeddings::PageEmbeddingsService* page_embeddings_service,
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
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback) {
  base::TimeTicks now = tick_clock_->NowTicks();

  AUTO_CONTEXT_LOG(base::StringPrintf("Processing query %s in mode %d", query,
                                      options.tab_selection_mode));

  if (!is_embedder_available_) {
    AUTO_CONTEXT_LOG("Embedder not available");
    RecordContextDeterminationStatus(
        ContextDeterminationStatus::kEmbedderNotAvailable);
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<content::WebContents*>({})));
    return;
  }

  // Force active tab embedding to be processed.
  page_embeddings_service_->ProcessAllEmbeddings();

  AUTO_CONTEXT_LOG("Submitted query to embedder");
  // TODO: crbug.com/452036470 - De-couple embeddings and recency signal
  // computation.
  embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUrgent, {query},
      base::BindOnce(&ContextualTasksContextService::OnQueryEmbeddingReady,
                     weak_ptr_factory_.GetWeakPtr(), query, options, now,
                     explicit_urls, std::move(callback)));
}

void ContextualTasksContextService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  is_embedder_available_ = metadata.IsValid();
}

passage_embeddings::PageEmbeddingsService::Priority
ContextualTasksContextService::GetDefaultPriority() const {
  return passage_embeddings::PageEmbeddingsService::Priority::kBackground;
}

void ContextualTasksContextService::OnQueryEmbeddingReady(
    const std::string& query,
    const TabSelectionOptions& options,
    base::TimeTicks start_time,
    const std::vector<GURL>& explicit_urls,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
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

  RecordContextDeterminationStatus(ContextDeterminationStatus::kSuccess);

  AUTO_CONTEXT_LOG(
      base::StringPrintf("Processing query embedding for %s", query));

  passage_embeddings::Embedding query_embedding = embeddings[0];
  std::vector<content::WebContents*> all_tabs = GetAllTabsForProfile(profile_);
  std::vector<content::WebContents*> relevant_tabs =
      SelectRelevantTabs(query, options, query_embedding, all_tabs);
  size_t initial_relevant_count = relevant_tabs.size();

  // Remove tabs that are not eligible for server upload.
  std::erase_if(relevant_tabs, [this](content::WebContents* web_contents) {
    bool removed = !ShouldAddTabToSelection(web_contents);
    if (removed) {
      AUTO_CONTEXT_LOG(
          base::StringPrintf("Removing %s from relevant set as it is not "
                             "eligible for server upload",
                             web_contents->GetLastCommittedURL().spec()));
    }
    return removed;
  });

  if (relevant_tabs.size() != initial_relevant_count) {
    base::UmaHistogramCounts100(
        "ContextualTasks.Context.RelevantButInvalidTabsCount",
        initial_relevant_count - relevant_tabs.size());
  }

  AUTO_CONTEXT_LOG(base::StringPrintf("Number of open tabs for query %s: %d",
                                      query, all_tabs.size()));
  AUTO_CONTEXT_LOG(base::StringPrintf(
      "Number of relevant tabs for query %s: %d", query, relevant_tabs.size()));

  base::UmaHistogramTimes("ContextualTasks.Context.ContextCalculationLatency",
                          tick_clock_->NowTicks() - start_time);
  base::UmaHistogramCounts100("ContextualTasks.Context.RelevantTabsCount",
                              relevant_tabs.size());

  if (!explicit_urls.empty()) {
    std::set<GURL> relevant_tab_url_set;
    for (auto* web_contents : relevant_tabs) {
      relevant_tab_url_set.insert(web_contents->GetLastCommittedURL());
    }
    RecordTabSelectionMetrics(
        relevant_tab_url_set,
        std::set<GURL>(explicit_urls.begin(), explicit_urls.end()));
  }

  std::move(callback).Run(std::move(relevant_tabs));
}

std::vector<content::WebContents*>
ContextualTasksContextService::SelectRelevantTabs(
    const std::string& query,
    const TabSelectionOptions& options,
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<content::WebContents*>& all_tabs) {
  switch (options.tab_selection_mode) {
    case mojom::TabSelectionMode::kMultiSignalScoring:
      return SelectTabsByMultiSignalScore(query, options, query_embedding,
                                          all_tabs);
    case mojom::TabSelectionMode::kEmbeddingsMatch:
      return SelectTabsByEmbeddingsMatch(query, options, query_embedding,
                                         all_tabs);
  }
}

std::vector<content::WebContents*>
ContextualTasksContextService::SelectTabsByMultiSignalScore(
    const std::string& query,
    const TabSelectionOptions& options,
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<content::WebContents*>& all_tabs) {
  std::vector<content::WebContents*> relevant_tabs;
  for (auto* web_contents : all_tabs) {
    // Collect tab signals.
    TabSignals tab_signals;
    tab_signals.web_contents = web_contents;
    tab_signals.embedding_score = GetBestEmbeddingScore(
        web_contents, query_embedding,
        page_embeddings_service_->GetEmbeddings(web_contents));
    tab_signals.duration_since_last_active =
        GetDurationSinceLastActive(web_contents);
    tab_signals.num_query_title_matching_words = GetMatchingWordsCount(
        query, base::UTF16ToUTF8(web_contents->GetTitle()));

    // Collect metrics.
    if (tab_signals.embedding_score.has_value()) {
      base::UmaHistogramCounts100(
          "ContextualTasks.Context.EmbeddingSimilarityScore",
          static_cast<int>(
              std::min(100 * *(tab_signals.embedding_score), 100.0f)));
    }
    if (tab_signals.duration_since_last_active.has_value()) {
      base::UmaHistogramTimes("ContextualTasks.Context.DurationSinceLastActive",
                              *(tab_signals.duration_since_last_active));
    }
    if (tab_signals.num_query_title_matching_words.has_value()) {
      base::UmaHistogramCounts100(
          "ContextualTasks.Context.MatchingWordsCount",
          std::min(*(tab_signals.num_query_title_matching_words), 100));
    }

    // Score and select qualifying tabs.
    double score = GetTabScore(tab_signals);
    if (score >= options.min_model_score.value_or(kMinMultiSignalScore.Get())) {
      relevant_tabs.push_back(tab_signals.web_contents);
    }

    base::UmaHistogramSparse("ContextualTasks.Context.TabScore",
                             static_cast<int>(std::min(100 * score, 100.0)));

    // Log for debugging.
    AUTO_CONTEXT_LOG(base::StringPrintf(
        "Query: %s | TabTitle: %s | EmbeddingsScore: %f | "
        "SecondsSinceLastActive: %d | MatchingWordsCount: %d | Score: %f",
        query, base::UTF16ToUTF8(web_contents->GetTitle()),
        tab_signals.embedding_score.value_or(0.0f),
        tab_signals.duration_since_last_active.has_value()
            ? tab_signals.duration_since_last_active->InSeconds()
            : -1,
        tab_signals.num_query_title_matching_words.value_or(0), score));
  }
  return relevant_tabs;
}

std::vector<content::WebContents*>
ContextualTasksContextService::SelectTabsByEmbeddingsMatch(
    const std::string& query,
    const TabSelectionOptions& options,
    const passage_embeddings::Embedding& query_embedding,
    const std::vector<content::WebContents*>& all_tabs) {
  std::vector<content::WebContents*> relevant_tabs;
  for (auto* web_contents : all_tabs) {
    std::vector<passage_embeddings::PassageEmbedding> web_contents_embeddings =
        page_embeddings_service_->GetEmbeddings(web_contents);
    AUTO_CONTEXT_LOG(base::StringPrintf(
        "Comparing query embedding to %llu embeddings for %s",
        web_contents_embeddings.size(),
        web_contents->GetLastCommittedURL().spec()));
    for (const auto& embedding : web_contents_embeddings) {
      if (kOnlyUseTitlesForSimilarity.Get() &&
          embedding.passage.second != passage_embeddings::PassageType::kTitle) {
        continue;
      }
      float similarity_score = embedding.embedding.ScoreWith(query_embedding);
      AUTO_CONTEXT_LOG(
          base::StringPrintf("Similarity with passage %s and query %s: %f",
                             embedding.passage.first, query, similarity_score));
      if (similarity_score >= options.min_model_score.value_or(
                                  kMinEmbeddingSimilarityScore.Get())) {
        relevant_tabs.push_back(web_contents);
        AUTO_CONTEXT_LOG(
            base::StringPrintf("Adding %s to relevant set",
                               web_contents->GetLastCommittedURL().spec()));
        break;
      }
    }
  }
  return relevant_tabs;
}

std::optional<base::TimeDelta>
ContextualTasksContextService::GetDurationSinceLastActive(
    content::WebContents* web_contents) {
  base::TimeDelta time_elapsed =
      tick_clock_->NowTicks() -
      std::max(web_contents->GetLastActiveTimeTicks(),
               web_contents->GetLastInteractionTimeTicks());

  if (time_elapsed.is_positive()) {
    return time_elapsed;
  }
  return std::nullopt;
}

bool ContextualTasksContextService::ShouldAddTabToSelection(
    content::WebContents* web_contents) {
  if (!page_content_extraction_service_) {
    // We don't know - err on the side of allowing the tab to be added.
    return true;
  }

  std::optional<page_content_annotations::ExtractedPageContentResult>
      extracted_page_content_result =
          page_content_extraction_service_
              ->GetExtractedPageContentAndEligibilityForPage(
                  web_contents->GetPrimaryPage());
  return !extracted_page_content_result ||
         extracted_page_content_result->is_eligible_for_server_upload;
}

}  // namespace contextual_tasks
