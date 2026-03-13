// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/task/sequenced_task_runner.h"

SearchPrewarmProgressService::SearchPrewarmProgressService() = default;

SearchPrewarmProgressService::~SearchPrewarmProgressService() = default;

bool SearchPrewarmProgressService::HasOnGoingSearchPrewarm() const {
  return !ongoing_prewarms_.empty();
}

bool SearchPrewarmProgressService::IsOnGoingSearchPrewarm(
    content::PrerenderHostId host_id) const {
  return ongoing_prewarms_.contains(host_id);
}

void SearchPrewarmProgressService::AddSearchPrewarmFinishedCallback(
    base::OnceClosure callback) {
  CHECK(HasOnGoingSearchPrewarm());
  on_finished_callbacks_.push_back(std::move(callback));
}

void SearchPrewarmProgressService::OnSearchPrewarmStarted(
    content::PrerenderHostId host_id) {
  ongoing_prewarms_.insert(host_id);
}

void SearchPrewarmProgressService::OnSearchPrewarmFinished(
    content::PrerenderHostId host_id) {
  CHECK(IsOnGoingSearchPrewarm(host_id));
  ongoing_prewarms_.erase(host_id);
  if (HasOnGoingSearchPrewarm()) {
    return;
  }
  // Copy the callbacks just in case the keyed service is
  // destructed in the callback.
  std::vector<base::OnceClosure> callbacks = std::move(on_finished_callbacks_);
  on_finished_callbacks_.clear();
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}
