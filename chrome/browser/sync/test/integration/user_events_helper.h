// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TEST_INTEGRATION_USER_EVENTS_HELPER_H_
#define CHROME_BROWSER_SYNC_TEST_INTEGRATION_USER_EVENTS_HELPER_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace fake_server {
class FakeServer;
}

namespace user_events_helper {

// Creates a test user event with specified event time.
sync_pb::UserEventSpecifics CreateTestEvent(base::Time time);

}  // namespace user_events_helper

class UserEventEqualityChecker : public SingleClientStatusChangeChecker {
 public:
  UserEventEqualityChecker(
      syncer::SyncServiceImpl* service,
      fake_server::FakeServer* fake_server,
      std::vector<sync_pb::UserEventSpecifics> expected_specifics);

  UserEventEqualityChecker(const UserEventEqualityChecker&) = delete;
  UserEventEqualityChecker& operator=(const UserEventEqualityChecker&) = delete;

  ~UserEventEqualityChecker() override;

  bool IsExitConditionSatisfied(std::ostream* os) override;

 private:
  const raw_ptr<fake_server::FakeServer> fake_server_;
  const std::vector<sync_pb::UserEventSpecifics> expected_specifics_;
};

#endif  // CHROME_BROWSER_SYNC_TEST_INTEGRATION_USER_EVENTS_HELPER_H_
