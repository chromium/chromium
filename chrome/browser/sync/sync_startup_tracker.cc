// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"

#include "base/functional/bind.h"
#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {

constexpr base::TimeDelta kDefaultWaitTimeout = base::Seconds(10);

absl::optional<base::TimeDelta> g_wait_timeout = kDefaultWaitTimeout;

}  // namespace

SyncStartupTracker::SyncStartupTracker(syncer::SyncService* sync_service,
                                       SyncStartupStateChangedCallback callback)
    : sync_service_(sync_service),
      sync_startup_status_changed_callback_(std::move(callback)) {
  DCHECK(sync_service_);
  sync_service_observation_.Observe(sync_service_);
  if (g_wait_timeout.has_value()) {
    timeout_waiter_.Start(FROM_HERE, g_wait_timeout.value(),
                          base::BindOnce(&SyncStartupTracker::OnStartupTimeout,
                                         weak_factory_.GetWeakPtr()));
  }

  CheckServiceState();
}

SyncStartupTracker::~SyncStartupTracker() = default;

void SyncStartupTracker::OnStateChanged(syncer::SyncService* sync) {
  CheckServiceState();
}

void SyncStartupTracker::OnStartupTimeout() {
  is_timed_out_ = true;
  CheckServiceState();
}

void SyncStartupTracker::CheckServiceState() {
  ServiceStartupState state = GetServiceStartupState(sync_service_);
  if (state == ServiceStartupState::kPending) {
    if (is_timed_out_) {
      state = ServiceStartupState::kTimeout;
    } else {
      // Do nothing - still waiting for sync to finish starting up.
      return;
    }
  }

  timeout_waiter_.Stop();
  sync_service_observation_.Reset();

  DCHECK(sync_startup_status_changed_callback_);
  std::move(sync_startup_status_changed_callback_).Run(state);
}

// static
SyncStartupTracker::ServiceStartupState
SyncStartupTracker::GetServiceStartupState(syncer::SyncService* sync_service) {
  // If no service exists or it can't be started, treat as a startup error.
  if (!sync_service || !sync_service->CanSyncFeatureStart()) {
    return ServiceStartupState::kError;
  }

  // If the sync engine has started up, notify the callback.
  if (sync_service->IsEngineInitialized()) {
    return ServiceStartupState::kComplete;
  }

  // If the sync service has some kind of error, report to the user.
  if (sync_service->HasUnrecoverableError()) {
    return ServiceStartupState::kError;
  }

  // If we have an auth error, exit.
  if (sync_service->GetAuthError().state() != GoogleServiceAuthError::NONE) {
    return ServiceStartupState::kError;
  }

  // No error detected yet, but the sync engine hasn't started up yet, so
  // we're in the pending state.
  return ServiceStartupState::kPending;
}

namespace testing {

ScopedSyncStartupTimeoutOverride::ScopedSyncStartupTimeoutOverride(
    absl::optional<base::TimeDelta> wait_timeout) {
  old_wait_timeout_ = g_wait_timeout;
  g_wait_timeout = wait_timeout;
}

ScopedSyncStartupTimeoutOverride::~ScopedSyncStartupTimeoutOverride() {
  g_wait_timeout = old_wait_timeout_;
}

}  // namespace testing
