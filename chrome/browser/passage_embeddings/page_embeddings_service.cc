// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/page_embeddings_service.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <utility>

#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace passage_embeddings {

namespace {
passage_embeddings::PassagePriority ConvertToPassagePriority(
    PageEmbeddingsService::Priority priority) {
  switch (priority) {
    case PageEmbeddingsService::kUserBlocking:
      return passage_embeddings::kUserInitiated;

    case PageEmbeddingsService::kUrgent:
      return passage_embeddings::kUrgent;

    case PageEmbeddingsService::kDefault:
      return passage_embeddings::kPassive;

    case PageEmbeddingsService::kBackground:
      return passage_embeddings::kLatent;
  }
}
}  // namespace

WebContentsDestructionObserver::WebContentsDestructionObserver(
    content::WebContents* web_contents,
    base::OnceCallback<void(content::WebContents*)> destroyed_callback)
    : WebContentsObserver(web_contents),
      destroyed_callback_(std::move(destroyed_callback)) {}

WebContentsDestructionObserver::~WebContentsDestructionObserver() = default;

void WebContentsDestructionObserver::WebContentsDestroyed() {
  std::move(destroyed_callback_).Run(web_contents());
}

PageEmbeddingsService::PageEmbeddingsService(
    EmbeddingCandidatesGenerator candidates_generator,
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service,
    passage_embeddings::Embedder* embedder)
    : candidates_generator_(candidates_generator), embedder_(embedder) {}

PageEmbeddingsService::~PageEmbeddingsService() = default;

void PageEmbeddingsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  UpdateTaskPriorities(GetActivePriority(observers_));
}

void PageEmbeddingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

  UpdateTaskPriorities(GetActivePriority(observers_));
}

std::vector<PassageEmbedding> PageEmbeddingsService::GetEmbeddings(
    content::WebContents* web_content) const {
  const auto loc = web_contents_state_.find(web_content);
  if (loc == web_contents_state_.end()) {
    return {};
  }
  return loc->second.passage_embeddings;
}

void PageEmbeddingsService::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  std::vector<std::string> passages =
      candidates_generator_.Run(page_content, 10);

  auto loc = web_contents_state_.find(web_contents);
  if (loc == web_contents_state_.end()) {
    web_contents_state_[web_contents].observer =
        std::make_unique<WebContentsDestructionObserver>(
            web_contents,
            base::BindOnce(&PageEmbeddingsService::OnWebContentsDestroyed,
                           // The observer's lifetime is less than this object.
                           base::Unretained(this)));
  }

  if (web_contents_state_[web_contents].active_task.has_value()) {
    embedder_->TryCancel(*web_contents_state_[web_contents].active_task);
    web_contents_state_[web_contents].active_task.reset();
  }

  Embedder::TaskId task_id = embedder_->ComputePassagesEmbeddings(
      ConvertToPassagePriority(current_priority_), std::move(passages),
      base::BindOnce(&PageEmbeddingsService::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(),
                     web_contents->GetWeakPtr()));

  web_contents_state_[web_contents].active_task = task_id;
}

void PageEmbeddingsService::OnEmbeddingsComputed(
    base::WeakPtr<content::WebContents> web_contents,
    std::vector<std::string> passages,
    std::vector<Embedding> embeddings,
    Embedder::TaskId task_id,
    ComputeEmbeddingsStatus status) {
  if (!web_contents) {
    // The web contents was destroyed while computing the embeddings.
    return;
  }

  CHECK_EQ(passages.size(), embeddings.size());

  std::vector<PassageEmbedding> passage_embeddings;
  for (size_t i = 0; i < passages.size(); ++i) {
    passage_embeddings.push_back(
        {std::move(passages[i]), std::move(embeddings[i])});
  }

  const auto loc = web_contents_state_.find(web_contents.get());
  DCHECK(loc != web_contents_state_.end());

  // Ignore stale embeddings from previously cancelled tasks.
  if (loc->second.active_task != task_id) {
    return;
  }

  loc->second.active_task.reset();
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    loc->second.passage_embeddings.clear();
    return;
  }
  loc->second.passage_embeddings = std::move(passage_embeddings);

  for (Observer& observer : observers_) {
    observer.OnPageEmbeddingsAvailable(web_contents.get());
  }
}

void PageEmbeddingsService::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  web_contents_state_.erase(web_contents);
}

// static
PageEmbeddingsService::Priority PageEmbeddingsService::GetActivePriority(
    const base::ObserverList<Observer>& observers) {
  return std::transform_reduce(
      observers.begin(), observers.end(), kDefault,
      [](Priority p1, Priority p2) { return std::min(p1, p2); },
      [](const Observer& observer) { return observer.GetDefaultPriority(); });
}

void PageEmbeddingsService::UpdateTaskPriorities(Priority priority) {
  if (priority == current_priority_) {
    return;
  }

  current_priority_ = priority;

  std::set<Embedder::TaskId> tasks;
  for (const auto& [web_contents, web_contents_state] : web_contents_state_) {
    if (web_contents_state.active_task.has_value()) {
      tasks.insert(*web_contents_state.active_task);
    }
  }

  if (!tasks.empty()) {
    embedder_->ReprioritizeTasks(ConvertToPassagePriority(priority), tasks);
  }
}

PageEmbeddingsService::WebContentsState::WebContentsState() = default;

PageEmbeddingsService::WebContentsState::~WebContentsState() = default;

}  // namespace passage_embeddings
