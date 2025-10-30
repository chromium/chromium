// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/contextual_tasks/public/features.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "content/public/browser/web_contents.h"

namespace contextual_tasks {

namespace {

// Convenience macro for emitting OPTIMIZATION_GUIDE_LOGs where
// optimization_keyed_service_ is defined.
#define AUTO_CONTEXT_LOG(message)                                            \
  OPTIMIZATION_GUIDE_LOG(                                                    \
      optimization_guide_common::mojom::LogSource::CONTEXTUAL_TASKS_CONTEXT, \
      optimization_guide_keyed_service_->GetOptimizationGuideLogger(),       \
      (message))

void RecordContextDeterminationStatus(ContextDeterminationStatus status) {
  base::UmaHistogramEnumeration(
      "ContextualTasks.Context.ContextDeterminationStatus", status);
}

}  // namespace

ContextualTasksContextService::ContextualTasksContextService(
    Profile* profile,
    passage_embeddings::PageEmbeddingsService* page_embeddings_service,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder,
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : profile_(profile),
      page_embeddings_service_(page_embeddings_service),
      embedder_metadata_provider_(embedder_metadata_provider),
      embedder_(embedder),
      optimization_guide_keyed_service_(optimization_guide_keyed_service) {
  scoped_observation_.Observe(embedder_metadata_provider_);
}

ContextualTasksContextService::~ContextualTasksContextService() = default;

void ContextualTasksContextService::GetRelevantTabsForQuery(
    const std::string& query,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback) {
  base::TimeTicks now = base::TimeTicks::Now();

  AUTO_CONTEXT_LOG(base::StringPrintf("Processing query %s", query));

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
  embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUrgent, {query},
      base::BindOnce(&ContextualTasksContextService::OnQueryEmbeddingReady,
                     weak_ptr_factory_.GetWeakPtr(), query, now,
                     std::move(callback)));
}

void ContextualTasksContextService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  is_embedder_available_ = metadata.IsValid();
}

void ContextualTasksContextService::OnQueryEmbeddingReady(
    const std::string& query,
    base::TimeTicks start_time,
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

  // Collect relevant web contents.
  // TODO: crbug.com/452056256 - Include other criteria other than embedding
  // score.
  std::vector<content::WebContents*> relevant_web_contents;
  int all_browsers_tab_count = 0;
  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [this, profile = profile_, &relevant_web_contents,
       &all_browsers_tab_count, &query_embedding,
       &query](BrowserWindowInterface* browser) {
        if (browser->GetProfile() != profile) {
          return true;
        }

        TabStripModel* const tab_strip_model = browser->GetTabStripModel();
        const int tab_count = tab_strip_model->count();
        all_browsers_tab_count += tab_count;
        for (int i = 0; i < tab_count; i++) {
          content::WebContents* web_contents =
              tab_strip_model->GetWebContentsAt(i);
          if (!web_contents) {
            continue;
          }

          if (!web_contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
            continue;
          }

          // See if any passage embeddings are closely related to the query
          // embedding. Just add if at least one is high enough.
          std::vector<passage_embeddings::PassageEmbedding>
              web_contents_embeddings =
                  page_embeddings_service_->GetEmbeddings(web_contents);
          AUTO_CONTEXT_LOG(base::StringPrintf(
              "Comparing query embedding to %llu embeddings for %s",
              web_contents_embeddings.size(),
              web_contents->GetLastCommittedURL().spec()));
          for (const auto& embedding : web_contents_embeddings) {
            if (kOnlyUseTitlesForSimilarity.Get() &&
                embedding.passage.second !=
                    passage_embeddings::PassageType::kTitle) {
              continue;
            }
            float similarity_score =
                embedding.embedding.ScoreWith(query_embedding);
            AUTO_CONTEXT_LOG(base::StringPrintf(
                "Similarity with passage %s and query %s: %f",
                embedding.passage.first, query, similarity_score));
            if (similarity_score > kMinEmbeddingSimilarityScore.Get()) {
              AUTO_CONTEXT_LOG(base::StringPrintf(
                  "Adding %s to relevant set",
                  web_contents->GetLastCommittedURL().spec()));
              relevant_web_contents.push_back(web_contents);
              break;
            }
          }
        }
        return true;
      });

  AUTO_CONTEXT_LOG(base::StringPrintf("Number of open tabs for query %s: %d",
                                        query, all_browsers_tab_count));
  AUTO_CONTEXT_LOG(
          base::StringPrintf("Number of relevant tabs for query %s: %d", query,
                             relevant_web_contents.size()));

  base::UmaHistogramTimes("ContextualTasks.Context.ContextCalculationLatency",
                          base::TimeTicks::Now() - start_time);
  base::UmaHistogramCounts100("ContextualTasks.Context.RelevantTabsCount",
                              relevant_web_contents.size());
  std::move(callback).Run(std::move(relevant_web_contents));
}

}  // namespace contextual_tasks
