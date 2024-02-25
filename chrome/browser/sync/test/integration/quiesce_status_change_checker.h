// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"

namespace syncer {
class SyncServiceImpl;
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
      std::vector<raw_ptr<syncer::SyncServiceImpl, VectorExperimental>>
          services);

  QuiesceStatusChangeChecker(const QuiesceStatusChangeChecker&) = delete;
  QuiesceStatusChangeChecker& operator=(const QuiesceStatusChangeChecker&) =
      delete;

  ~QuiesceStatusChangeChecker() override;

  // Implementation of StatusChangeChecker.
  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  class NestedUpdatedProgressMarkerChecker;
  std::vector<std::unique_ptr<NestedUpdatedProgressMarkerChecker>> checkers_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_QUIESCE_STATUS_CHANGE_CHECKER_H_
