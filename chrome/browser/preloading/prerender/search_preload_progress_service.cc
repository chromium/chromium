// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prerender/search_preload_progress_service.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/preloading/preloading_features.h"

SearchPreloadProgressService::SearchPreloadProgressService() = default;

SearchPreloadProgressService::~SearchPreloadProgressService() = default;

bool SearchPreloadProgressService::HasOnGoingSearchPrewarm() const {
  return !ongoing_prewarms_.empty();
}

bool SearchPreloadProgressService::IsOnGoingSearchPrewarm(
    content::PrerenderHostId host_id) const {
  return ongoing_prewarms_.contains(host_id);
}

bool SearchPreloadProgressService::ShouldThrottleSearchPreloads() const {
  if (!base::FeatureList::IsEnabled(features::kPrewarm)) {
    return false;
  }
  if (!features::kPrewarmThrottlePrefetch.Get()) {
    return false;
  }
  return HasOnGoingSearchPrewarm();
}

base::CallbackListSubscription
SearchPreloadProgressService::RegisterSearchPrewarmFinishedCallback(
    base::RepeatingClosure callback) {
  return callbacks_.Add(std::move(callback));
}

base::WeakPtr<SearchPreloadProgressService>
SearchPreloadProgressService::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SearchPreloadProgressService::OnSearchPrewarmStarted(
    content::PrerenderHostId host_id) {
  ongoing_prewarms_.insert(host_id);
}

void SearchPreloadProgressService::OnSearchPrewarmFinished(
    content::PrerenderHostId host_id,
    content::PrerenderLifecycleStatus status) {
  CHECK(IsOnGoingSearchPrewarm(host_id));
  ongoing_prewarms_.erase(host_id);

  if (status == content::PrerenderLifecycleStatus::kHttpBadResponse) {
    EnterBlackoutPeriod();
  }

  if (HasOnGoingSearchPrewarm()) {
    return;
  }

  callbacks_.Notify();
}

void SearchPreloadProgressService::EnterBlackoutPeriod() {
  disabled_until_ = base::TimeTicks::Now() +
                    base::Seconds(features::kMaxBlackoutDurationSeconds.Get());
}

bool SearchPreloadProgressService::ShouldBlockPrewarm() const {
  return base::TimeTicks::Now() < disabled_until_;
}
