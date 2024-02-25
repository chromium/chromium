// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

#include "base/memory/raw_ptr.h"

MultiClientStatusChangeChecker::MultiClientStatusChangeChecker(
    std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>> services)
    : services_(services) {
  for (syncer::SyncServiceImpl* service : services) {
    scoped_observations_.AddObservation(service);
  }
}

MultiClientStatusChangeChecker::~MultiClientStatusChangeChecker() = default;

void MultiClientStatusChangeChecker::OnStateChanged(syncer::SyncService* sync) {
  CheckExitCondition();
}

void MultiClientStatusChangeChecker::OnSyncShutdown(syncer::SyncService* sync) {
  scoped_observations_.RemoveObservation(sync);
}
