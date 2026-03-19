// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_prewarm_progress_service.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/preloading_features.h"

SearchPrewarmProgressService::SearchPrewarmProgressService() = default;

SearchPrewarmProgressService::~SearchPrewarmProgressService() = default;

bool SearchPrewarmProgressService::HasOnGoingSearchPrewarm() const {
  return !ongoing_prewarms_.empty();
}

bool SearchPrewarmProgressService::IsOnGoingSearchPrewarm(
    content::PrerenderHostId host_id) const {
  return ongoing_prewarms_.contains(host_id);
}

bool SearchPrewarmProgressService::ShouldThrottleSearchPreloads() const {
  if (!base::FeatureList::IsEnabled(features::kPrewarm)) {
    return false;
  }
  if (!features::kPrewarmThrottlePrefetch.Get()) {
    return false;
  }
  return HasOnGoingSearchPrewarm();
}

base::CallbackListSubscription
SearchPrewarmProgressService::RegisterSearchPrewarmFinishedCallback(
    base::RepeatingClosure callback) {
  return callbacks_.Add(std::move(callback));
}

base::WeakPtr<SearchPrewarmProgressService>
SearchPrewarmProgressService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
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

  callbacks_.Notify();
}
