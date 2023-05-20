// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_

#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "components/sync/service/sync_service_observer.h"

namespace syncer {
class SyncServiceImpl;
}  // namespace syncer

// This class provides some common functionality for StatusChangeCheckers that
// observe only one SyncServiceImpl.  This class is abstract.  Its
// descendants are expected to provide additional functionality.
class SingleClientStatusChangeChecker : public MultiClientStatusChangeChecker {
 public:
  explicit SingleClientStatusChangeChecker(syncer::SyncServiceImpl* service);
  ~SingleClientStatusChangeChecker() override;

  syncer::SyncServiceImpl* service();
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
