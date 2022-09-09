// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sync_engine_stopped_checker.h"

namespace syncer {

SyncEngineStoppedChecker::SyncEngineStoppedChecker(
    syncer::SyncServiceImpl* service)
    : SingleClientStatusChangeChecker(service) {}

// StatusChangeChecker implementation.
bool SyncEngineStoppedChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for sync to stop";
  return !service()->IsEngineInitialized();
}

}  // namespace syncer
