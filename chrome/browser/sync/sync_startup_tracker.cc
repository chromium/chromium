// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "components/sync/service/sync_service.h"

namespace {

constexpr base::TimeDelta kDefaultWaitTimeout = base::Seconds(10);

std::optional<base::TimeDelta> g_wait_timeout = kDefaultWaitTimeout;

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

  // Unrecoverable errors return false for CanSyncFeatureStart(), handled above.
  DCHECK(!sync_service->HasUnrecoverableError());

  switch (sync_service->GetTransportState()) {
    case syncer::SyncService::TransportState::DISABLED:
      NOTREACHED_IN_MIGRATION();
      break;
    case syncer::SyncService::TransportState::START_DEFERRED:
    case syncer::SyncService::TransportState::INITIALIZING:
      // No error detected yet, but the sync engine hasn't started up yet, so
      // we're in the pending state.
      return ServiceStartupState::kPending;
    case syncer::SyncService::TransportState::PAUSED:
      // Persistent auth errors lead to sync pausing.
      return ServiceStartupState::kError;
    case syncer::SyncService::TransportState::PENDING_DESIRED_CONFIGURATION:
    case syncer::SyncService::TransportState::CONFIGURING:
    case syncer::SyncService::TransportState::ACTIVE:
      DCHECK(sync_service->IsEngineInitialized());
      return ServiceStartupState::kComplete;
  }

  NOTREACHED_IN_MIGRATION();
  return ServiceStartupState::kError;
}

namespace testing {

ScopedSyncStartupTimeoutOverride::ScopedSyncStartupTimeoutOverride(
    std::optional<base::TimeDelta> wait_timeout) {
  old_wait_timeout_ = g_wait_timeout;
  g_wait_timeout = wait_timeout;
}

ScopedSyncStartupTimeoutOverride::~ScopedSyncStartupTimeoutOverride() {
  g_wait_timeout = old_wait_timeout_;
}

}  // namespace testing
