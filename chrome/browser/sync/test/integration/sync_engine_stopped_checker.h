// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ENGINE_STOPPED_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ENGINE_STOPPED_CHECKER_H_

#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"

namespace syncer {

// A helper class to wait for sync engine being stopped. Common usecase is to
// wait for CLIENT_DATA_OBSOLETE being handled.
class SyncEngineStoppedChecker : public SingleClientStatusChangeChecker {
 public:
  explicit SyncEngineStoppedChecker(syncer::SyncServiceImpl* service);
  SyncEngineStoppedChecker(const SyncEngineStoppedChecker&) = delete;
  SyncEngineStoppedChecker& operator=(const SyncEngineStoppedChecker&) = delete;
  ~SyncEngineStoppedChecker() override = default;

  // StatusChangeChecker implementation.
  bool IsExitConditionSatisfied(std::ostream* os) override;
};

}  // namespace syncer

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SYNC_ENGINE_STOPPED_CHECKER_H_
