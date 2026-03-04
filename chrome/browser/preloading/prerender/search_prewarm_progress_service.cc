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
  return ongoing_prewarms_ > 0;
}

void SearchPrewarmProgressService::AddSearchPrewarmFinishedCallback(
    base::OnceClosure callback) {
  CHECK(HasOnGoingSearchPrewarm());
  on_finished_callbacks_.push_back(std::move(callback));
}

void SearchPrewarmProgressService::OnSearchPrewarmStarted() {
  ongoing_prewarms_++;
}

void SearchPrewarmProgressService::OnSearchPrewarmFinished() {
  CHECK_GT(ongoing_prewarms_, 0);
  ongoing_prewarms_--;
  if (ongoing_prewarms_) {
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
