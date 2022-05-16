// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_startup_tracker.h"

#include "components/sync/driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"

SyncStartupTracker::SyncStartupTracker(syncer::SyncService* sync_service,
                                       Observer* observer)
    : sync_service_(sync_service), observer_(observer) {
  if (sync_service_) {
    sync_service_->AddObserver(this);
  }

  CheckServiceState();
}

SyncStartupTracker::~SyncStartupTracker() {
  if (sync_service_) {
    sync_service_->RemoveObserver(this);
  }
}

void SyncStartupTracker::OnStateChanged(syncer::SyncService* sync) {
  CheckServiceState();
}

void SyncStartupTracker::CheckServiceState() {
  // Note: the observer may free this object so it is not allowed to access
  // this object after invoking the observer callback below.
  switch (GetSyncServiceState(sync_service_)) {
    case SYNC_STARTUP_ERROR:
      observer_->SyncStartupFailed();
      break;
    case SYNC_STARTUP_COMPLETE:
      observer_->SyncStartupCompleted();
      break;
    case SYNC_STARTUP_PENDING:
      // Do nothing - still waiting for sync to finish starting up.
      break;
  }
}

// static
SyncStartupTracker::SyncServiceState SyncStartupTracker::GetSyncServiceState(
    syncer::SyncService* sync_service) {
  // If no service exists or it can't be started, treat as a startup error.
  if (!sync_service || !sync_service->CanSyncFeatureStart()) {
    return SYNC_STARTUP_ERROR;
  }

  // If the sync engine has started up, notify the callback.
  if (sync_service->IsEngineInitialized()) {
    return SYNC_STARTUP_COMPLETE;
  }

  // If the sync service has some kind of error, report to the user.
  if (sync_service->HasUnrecoverableError()) {
    return SYNC_STARTUP_ERROR;
  }

  // If we have an auth error, exit.
  if (sync_service->GetAuthError().state() != GoogleServiceAuthError::NONE) {
    return SYNC_STARTUP_ERROR;
  }

  // No error detected yet, but the sync engine hasn't started up yet, so
  // we're in the pending state.
  return SYNC_STARTUP_PENDING;
}
