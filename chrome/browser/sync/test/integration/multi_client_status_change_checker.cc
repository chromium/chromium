// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

#include "base/logging.h"

MultiClientStatusChangeChecker::MultiClientStatusChangeChecker(
    std::vector<syncer::ProfileSyncService*> services)
    : services_(services) {
  for (syncer::ProfileSyncService* service : services)
    scoped_observer_.Add(service);
}

MultiClientStatusChangeChecker::~MultiClientStatusChangeChecker() = default;

void MultiClientStatusChangeChecker::OnStateChanged(syncer::SyncService* sync) {
  CheckExitCondition();
}
