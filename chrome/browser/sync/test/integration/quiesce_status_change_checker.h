// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

namespace syncer {
class ProfileSyncService;
}  // namespace syncer

// Waits until all provided clients have finished committing any unsynced items
// and downloading each others' udpates.
//
// This requires that "self-notifications" be enabled.  Otherwise the clients
// will not fetch the latest progress markers on their own, and the latest
// progress markers are needed to confirm that clients are in sync.
class QuiesceStatusChangeChecker : public MultiClientStatusChangeChecker {
 public:
  explicit QuiesceStatusChangeChecker(
      std::vector<syncer::ProfileSyncService*> services);
  ~QuiesceStatusChangeChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  class NestedUpdatedProgressMarkerChecker;
  std::vector<std::unique_ptr<NestedUpdatedProgressMarkerChecker>> checkers_;

  DISALLOW_COPY_AND_ASSIGN(QuiesceStatusChangeChecker);
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_
