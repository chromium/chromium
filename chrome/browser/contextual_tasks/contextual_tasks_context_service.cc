// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_service.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/passage_embeddings/page_embeddings_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/passage_embeddings/passage_embeddings_types.h"

namespace contextual_tasks {

ContextualTasksContextService::ContextualTasksContextService(
    Profile* profile,
    passage_embeddings::PageEmbeddingsService* page_embeddings_service,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider,
    passage_embeddings::Embedder* embedder)
    : profile_(profile),
      page_embeddings_service_(page_embeddings_service),
      embedder_metadata_provider_(embedder_metadata_provider),
      embedder_(embedder) {
  scoped_observation_.Observe(embedder_metadata_provider_);
}

ContextualTasksContextService::~ContextualTasksContextService() = default;

void ContextualTasksContextService::GetRelevantTabsForQuery(
    const std::string& query,
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback) {
  if (!is_embedder_available_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<content::WebContents*>({})));
    return;
  }

  // Force active tab embedding to be processed.
  page_embeddings_service_->ProcessAllEmbeddings();

  embedder_->ComputePassagesEmbeddings(
      passage_embeddings::PassagePriority::kUrgent, {query},
      base::BindOnce(&ContextualTasksContextService::OnQueryEmbeddingReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ContextualTasksContextService::EmbedderMetadataUpdated(
    passage_embeddings::EmbedderMetadata metadata) {
  is_embedder_available_ = metadata.IsValid();
}

void ContextualTasksContextService::OnQueryEmbeddingReady(
    base::OnceCallback<void(std::vector<content::WebContents*>)> callback,
    std::vector<std::string> passages,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  // Query embedding was not successfully generated.
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    std::move(callback).Run({});
    return;
  }
  // Unexpected output size. Just return.
  if (embeddings.size() != 1u) {
    std::move(callback).Run({});
    return;
  }

  passage_embeddings::Embedding query_embedding = embeddings[0];

  // Collect relevant web contents.
  // TODO: crbug.com/452056256 - Include other criteria other than embedding
  // score.
  std::vector<content::WebContents*> relevant_web_contents;
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (!browser || browser->profile() != profile_) {
      continue;
    }

    TabStripModel* tab_strip_model = browser->tab_strip_model();
    int tab_count = tab_strip_model->count();
    for (int i = 0; i < tab_count; i++) {
      content::WebContents* web_contents = tab_strip_model->GetWebContentsAt(i);
      if (!web_contents) {
        continue;
      }

      // See if any passage embeddings are closely related to the query
      // embedding. Just add if at least one is high enough.
      std::vector<passage_embeddings::PassageEmbedding>
          web_contents_embeddings =
              page_embeddings_service_->GetEmbeddings(web_contents);
      for (const auto& embedding : web_contents_embeddings) {
        // TODO: crbug.com/452056256 - Make comparing score configurable.
        if (embedding.embedding.ScoreWith(query_embedding) > 0.5) {
          relevant_web_contents.push_back(web_contents);
          break;
        }
      }
    }
  }
  std::move(callback).Run(std::move(relevant_web_contents));
}

}  // namespace contextual_tasks
