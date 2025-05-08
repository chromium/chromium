// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/web_contents_passage_embedder.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/browser/web_contents.h"

namespace passage_embeddings {

WebContentsPassageEmbedder::WebContentsPassageEmbedder(
    content::WebContents* web_contents,
    Delegate& delegate)
    : WebContentsObserver(web_contents), delegate_(delegate) {}

WebContentsPassageEmbedder::~WebContentsPassageEmbedder() {
  if (current_task_id_) {
    delegate_->TryCancel(*current_task_id_);
  }
}

void WebContentsPassageEmbedder::WebContentsDestroyed() {
  delegate_->OnWebContentsDestroyed(web_contents());
  // The object may be destroyed at this point.
}

void WebContentsPassageEmbedder::ComputeEmbeddings(
    std::vector<std::string> passages) {
  if (current_task_id_) {
    delegate_->TryCancel(*current_task_id_);
  }

  current_task_id_ = delegate_->ComputePassagesEmbeddings(
      std::move(passages),
      base::BindOnce(&WebContentsPassageEmbedder::OnPassageEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebContentsPassageEmbedder::OnPassageEmbeddingsComputed(
    std::vector<std::string> passages,
    std::vector<Embedding> embeddings,
    Embedder::TaskId task_id,
    ComputeEmbeddingsStatus status) {
  VLOG(2) << "Got " << embeddings.size() << " embeddings with status "
          << static_cast<int>(status) << " for task " << task_id;
  if (task_id == current_task_id_) {
    current_task_id_.reset();
  } else {
    if (status == ComputeEmbeddingsStatus::kSuccess) {
      base::UmaHistogramCounts100("History.Embeddings.StaleComputedEmbeddings",
                                  embeddings.size());
    }
  }
}

WebContentsImmediatePassageEmbedder::WebContentsImmediatePassageEmbedder(
    content::WebContents* web_contents,
    Delegate& delegate)
    : WebContentsPassageEmbedder(web_contents, delegate) {}

void WebContentsImmediatePassageEmbedder::AcceptPassages(
    std::vector<std::string> passages) {
  ComputeEmbeddings(std::move(passages));
}

WebContentsBackgroundPassageEmbedder::WebContentsBackgroundPassageEmbedder(
    content::WebContents* web_contents,
    Delegate& delegate)
    : WebContentsPassageEmbedder(web_contents, delegate) {}

WebContentsBackgroundPassageEmbedder::~WebContentsBackgroundPassageEmbedder() =
    default;

void WebContentsBackgroundPassageEmbedder::OnVisibilityChanged(
    content::Visibility visibility) {
  web_contents_visibility_ = visibility;
  // Compute embeddings for any pending passages when transitioning to HIDDEN
  // visibility.
  if (web_contents_visibility_ == content::Visibility::HIDDEN &&
      !pending_passages_.empty()) {
    ComputeEmbeddings(std::move(pending_passages_));
    pending_passages_.clear();
  }
}

void WebContentsBackgroundPassageEmbedder::AcceptPassages(
    std::vector<std::string> passages) {
  if (web_contents_visibility_ == content::Visibility::HIDDEN) {
    // The WebContents may have transitioned from VISIBLE to HIDDEN by the time
    // we received the passages, so compute embeddings.
    ComputeEmbeddings(std::move(passages));
    pending_passages_.clear();
  } else {
    // Store the passages for a possible later embeddings computation on
    // transition to HIDDEN.
    pending_passages_ = passages;
  }
}

void WebContentsBackgroundPassageEmbedder::
    MaybeProcessPendingPassagesOnPriorityIncrease() {
  // Force processing of passages for non-hidden tabs, since we need embeddings
  // for all WebContents the user has seen at high priority.
  if (!pending_passages_.empty() &&
      web_contents()->GetVisibility() != content::Visibility::HIDDEN) {
    ComputeEmbeddings(std::move(pending_passages_));
    pending_passages_.clear();
  }
}

}  // namespace passage_embeddings
