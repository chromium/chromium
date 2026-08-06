// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/loader/fetch_keepalive_process_manager.h"

#include "base/check.h"
#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"

FetchKeepAliveProcessManager::FetchKeepAliveProcessManager() = default;
FetchKeepAliveProcessManager::~FetchKeepAliveProcessManager() = default;

void FetchKeepAliveProcessManager::OnRequestCreated(Profile& profile) {
  ++num_loaders_;
  if (num_loaders_ > max_concurrent_) {
    max_concurrent_ = num_loaders_;
  }
  if (num_loaders_ == 1 &&
      !KeepAliveRegistry::GetInstance()->IsShuttingDown()) {
    // The first loader always finds no existing process hold: any prior hold
    // was released when the loader count last dropped to zero.
    CHECK(!process_keep_alive_);
    process_keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::FETCH_KEEPALIVE_REQUEST,
        KeepAliveRestartOption::DISABLED);
    hold_started_ = base::TimeTicks::Now();
  }
  // Hold every profile that has in-flight keepalive loaders, not just the one
  // that started the process-wide hold above: releasing a profile destroys its
  // StoragePartition and aborts that profile's loaders.
  ProfileHold& hold = profile_holds_[&profile];
  ++hold.num_loaders;
  if (hold.num_loaders == 1) {
    hold.profile_keep_alive = ScopedProfileKeepAlive::TryAcquire(
        profile.GetOriginalProfile(), ProfileKeepAliveOrigin::kFetchKeepAlive);
  }
}

void FetchKeepAliveProcessManager::OnRequestDestroyed(Profile& profile) {
  // Each loader's creation was counted (OnRequestCreated runs for every loader
  // while the feature is on), so its destruction always has a live loader to
  // decrement.
  CHECK_GT(num_loaders_, 0u);
  --num_loaders_;

  // `profile` may already be halfway through destruction when its loaders are
  // torn down with their StoragePartition, so it is only used as a lookup key
  // here and never dereferenced.
  auto it = profile_holds_.find(&profile);
  if (it != profile_holds_.end()) {
    --it->second.num_loaders;
    if (it->second.num_loaders == 0) {
      profile_holds_.erase(it);
    }
  }

  if (num_loaders_ == 0) {
    if (!process_keep_alive_) {
      // Request creation can race with browser shutdown, after the registry has
      // stopped accepting new keep-alives.
      max_concurrent_ = 0;
      return;
    }

    const bool held_past_last_window_close =
        GlobalBrowserCollection::GetInstance() &&
        GlobalBrowserCollection::GetInstance()->IsEmpty();
    base::UmaHistogramBoolean(
        "Chrome.FetchKeepAlive.ProcessAlive.HeldPastLastWindowClose",
        held_past_last_window_close);
    base::UmaHistogramMediumTimes(
        "Chrome.FetchKeepAlive.ProcessAlive.HoldDuration",
        base::TimeTicks::Now() - hold_started_);
    base::UmaHistogramCounts100(
        "Chrome.FetchKeepAlive.ProcessAlive.MaxConcurrentLoaders",
        static_cast<int>(max_concurrent_));
    hold_started_ = base::TimeTicks();
    max_concurrent_ = 0;
    process_keep_alive_.reset();
  }
}
