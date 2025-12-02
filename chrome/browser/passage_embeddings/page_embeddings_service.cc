// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/page_embeddings_service.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <utility>

#include "chrome/browser/passage_embeddings/embeddings_candidate_generator.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
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

PassageEmbedding::PassageEmbedding() = default;
PassageEmbedding::~PassageEmbedding() = default;
PassageEmbedding::PassageEmbedding(const PassageEmbedding& other) = default;
PassageEmbedding::PassageEmbedding(std::pair<std::string, PassageType> passage,
                                   Embedding embedding)
    : passage(std::move(passage)), embedding(std::move(embedding)) {}

class PageEmbeddingsService::WebContentsEventsObserver
    : public content::WebContentsObserver {
 public:
  WebContentsEventsObserver(content::WebContents* web_contents,
                            PageEmbeddingsService* page_embeddings_service)
      : WebContentsObserver(web_contents),
        page_embeddings_service_(page_embeddings_service) {}
  ~WebContentsEventsObserver() override = default;

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN) {
      page_embeddings_service_->ComputeEmbeddings(web_contents());
    }
  }

  void WebContentsDestroyed() override {
    page_embeddings_service_->web_contents_state_.erase(web_contents());
  }

  bool IsWebContentsHidden() const {
    return web_contents()->GetVisibility() == content::Visibility::HIDDEN;
  }

 private:
  raw_ptr<PageEmbeddingsService> page_embeddings_service_;
};

struct PageEmbeddingsService::WebContentsState {
  WebContentsState();
  ~WebContentsState();

  std::unique_ptr<WebContentsEventsObserver> observer;

  // pending_passages is non-empty from the time passages are produced via
  // candidates_generator_ to the time that embeddings are requested.
  std::vector<std::pair<std::string, PassageType>> pending_passages;

  // The currently active task for computing embeddings. Non-empty while the
  // embedding computation is pending.
  std::optional<Embedder::TaskId> active_task;

  // passage_embeddings is empty until embeddings are received.
  std::vector<PassageEmbedding> passage_embeddings;
};

PageEmbeddingsService::ScopedPriority::ScopedPriority(
    PageEmbeddingsService* service,
    Observer* observer,
    Priority priority)
    : service_(service), observer_(observer) {
  // Only one scoped priority per observer is supported.
  DCHECK_EQ(0u, service_->temporary_priority_.count(observer));

  // We only support raising the priority.
  DCHECK_LT(priority, observer->GetDefaultPriority());

  service_->temporary_priority_[observer] = priority;

  if (priority < service_->current_priority_) {
    service_->current_priority_ = priority;
    service_->UpdateTaskPriorities(service_->current_priority_);
  }
}

PageEmbeddingsService::ScopedPriority::~ScopedPriority() {
  if (!service_) {
    // The object has been moved-from.
    return;
  }

  service_->temporary_priority_.erase(observer_);

  Priority next_priority =
      GetActivePriority(service_->observers_, service_->temporary_priority_);
  if (next_priority != service_->current_priority_) {
    service_->current_priority_ = next_priority;
    service_->UpdateTaskPriorities(service_->current_priority_);
  }
}

PageEmbeddingsService::ScopedPriority::ScopedPriority(ScopedPriority&& other) {
  *this = std::move(other);
}

PageEmbeddingsService::ScopedPriority&
PageEmbeddingsService::ScopedPriority::operator=(ScopedPriority&& other) {
  service_ = other.service_;
  observer_ = other.observer_;

  other.service_ = nullptr;
  other.observer_ = nullptr;

  return *this;
}

PageEmbeddingsService::PageEmbeddingsService(
    EmbeddingCandidatesGenerator candidates_generator,
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service,
    passage_embeddings::Embedder* embedder)
    : candidates_generator_(candidates_generator),
      embedder_(embedder),
      page_content_extraction_service_(page_content_extraction_service) {}

PageEmbeddingsService::PageEmbeddingsService(
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service)
    : PageEmbeddingsService(base::BindRepeating(&GenerateEmbeddingsCandidates),
                            page_content_extraction_service,
                            nullptr) {}

PageEmbeddingsService::~PageEmbeddingsService() = default;

void PageEmbeddingsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  if (!page_content_extraction_observation_.IsObserving()) {
    page_content_extraction_observation_.Observe(
        page_content_extraction_service_);
  }

  UpdateTaskPriorities(GetActivePriority(observers_, temporary_priority_));
}

void PageEmbeddingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

  if (observers_.empty() &&
      page_content_extraction_observation_.IsObserving()) {
    page_content_extraction_observation_.Reset();
  }

  UpdateTaskPriorities(GetActivePriority(observers_, temporary_priority_));
}

PageEmbeddingsService::ScopedPriority PageEmbeddingsService::RaisePriority(
    Observer* observer,
    Priority priority) {
  return ScopedPriority(this, observer, priority);
}

void PageEmbeddingsService::ProcessAllEmbeddings() {
  // For the computation of embeddings for all visible tabs, which are otherwise
  // only lazily computed on being hidden.
  for (const auto& [web_contents, web_contents_state] : web_contents_state_) {
    if (!web_contents_state.observer->IsWebContentsHidden() &&
        !web_contents_state.pending_passages.empty()) {
      ComputeEmbeddings(web_contents);
    }
  }
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

  auto loc = web_contents_state_.find(web_contents);
  if (loc == web_contents_state_.end()) {
    web_contents_state_[web_contents].observer =
        std::make_unique<WebContentsEventsObserver>(web_contents, this);
  }

  web_contents_state_[web_contents].pending_passages =
      candidates_generator_.Run(page_content, kMaxPassagesPerPage.Get());

  if (web_contents_state_[web_contents].observer->IsWebContentsHidden()) {
    // The WebContents may have transitioned from visible to hidden by the time
    // we received the passages, so compute embeddings.
    ComputeEmbeddings(web_contents);
  }
}

void PageEmbeddingsService::ComputeEmbeddings(
    content::WebContents* web_contents) {
  WebContentsState& state = web_contents_state_[web_contents];
  if (state.active_task.has_value()) {
    embedder_->TryCancel(*state.active_task);
    state.active_task.reset();
  }

  // Ensure that state.pending_passages is cleared before invoking
  // ComputePassagesEmbeddings().
  std::vector<std::pair<std::string, PassageType>> pending_passages;
  pending_passages.swap(state.pending_passages);

  std::vector<PassageType> passage_types;
  passage_types.reserve(pending_passages.size());
  std::vector<std::string> string_passages;
  string_passages.reserve(pending_passages.size());
  for (const auto& passage : pending_passages) {
    string_passages.push_back(passage.first);
    passage_types.push_back(passage.second);
  }

  state.active_task = embedder_->ComputePassagesEmbeddings(
      ConvertToPassagePriority(current_priority_), std::move(string_passages),
      base::BindOnce(&PageEmbeddingsService::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(passage_types),
                     web_contents->GetWeakPtr()));
}

void PageEmbeddingsService::OnEmbeddingsComputed(
    std::vector<PassageType> passage_types,
    base::WeakPtr<content::WebContents> web_contents,
    std::vector<std::string> passage_strings,
    std::vector<Embedding> embeddings,
    Embedder::TaskId task_id,
    ComputeEmbeddingsStatus status) {
  if (!web_contents) {
    // The web contents was destroyed while computing the embeddings.
    return;
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

  CHECK_EQ(passage_types.size(), embeddings.size());
  CHECK_EQ(passage_strings.size(), embeddings.size());

  std::vector<PassageEmbedding> passage_embeddings;
  for (size_t i = 0; i < passage_types.size(); ++i) {
    passage_embeddings.emplace_back(
        std::make_pair(std::move(passage_strings[i]),
                       std::move(passage_types[i])),
        std::move(embeddings[i]));
  }
  loc->second.passage_embeddings = std::move(passage_embeddings);

  for (Observer& observer : observers_) {
    observer.OnPageEmbeddingsAvailable(web_contents.get());
  }
}

// static
PageEmbeddingsService::Priority PageEmbeddingsService::GetActivePriority(
    const base::ObserverList<Observer>& observers,
    const std::map<Observer*, Priority>& temporary_priority) {
  const Priority highest_default_priority = std::transform_reduce(
      observers.begin(), observers.end(), kDefault,
      [](Priority p1, Priority p2) { return std::min(p1, p2); },
      [](const Observer& observer) { return observer.GetDefaultPriority(); });

  return std::transform_reduce(
      temporary_priority.begin(), temporary_priority.end(),
      highest_default_priority,
      [](Priority p1, Priority p2) { return std::min(p1, p2); },
      [](const std::map<Observer*, Priority>::value_type& pair) {
        return pair.second;
      });
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
