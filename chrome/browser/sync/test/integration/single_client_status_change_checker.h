// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/multi_client_status_change_checker.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class ProfileSyncService;
}  // namespace syncer

// This class provides some common functionality for StatusChangeCheckers that
// observe only one ProfileSyncService.  This class is abstract.  Its
// descendants are expected to provide additional functionality.
class SingleClientStatusChangeChecker
  : public MultiClientStatusChangeChecker {
 public:
  explicit SingleClientStatusChangeChecker(syncer::ProfileSyncService* service);
  ~SingleClientStatusChangeChecker() override;

  // StatusChangeChecker implementations and stubs.
  bool IsExitConditionSatisfied(std::ostream* os) override = 0;

  syncer::ProfileSyncService* service();
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_SINGLE_CLIENT_STATUS_CHANGE_CHECKER_H_
