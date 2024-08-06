// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/user_events_helper.h"

#include "base/ranges/algorithm.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "components/sync/test/fake_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using fake_server::FakeServer;
using sync_pb::SyncEntity;
using sync_pb::UserEventSpecifics;

namespace user_events_helper {

UserEventSpecifics CreateTestEvent(base::Time time) {
  UserEventSpecifics specifics;
  specifics.set_event_time_usec(
      time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.set_navigation_id(time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  specifics.mutable_test_event();
  return specifics;
}

}  // namespace user_events_helper

UserEventEqualityChecker::UserEventEqualityChecker(
    syncer::SyncServiceImpl* service,
    FakeServer* fake_server,
    std::vector<UserEventSpecifics> expected_specifics)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_specifics_(expected_specifics) {}

UserEventEqualityChecker::~UserEventEqualityChecker() = default;

bool UserEventEqualityChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting server side USER_EVENTS to match expected.";

  std::vector<SyncEntity> entities =
      fake_server_->GetSyncEntitiesByDataType(syncer::USER_EVENTS);

  // |entities.size()| is only going to grow, if |entities.size()| ever
  // becomes bigger then all hope is lost of passing, stop now.
  EXPECT_GE(expected_specifics_.size(), entities.size());

  if (expected_specifics_.size() > entities.size()) {
    return false;
  }

  // Number of events on server matches expected, exit condition is satisfied.
  // Let's verify that content matches as well.

  // Make a copy of |expected_specifics_| so that we can safely modify it.
  std::vector<sync_pb::UserEventSpecifics> remaining_expected_specifics =
      expected_specifics_;
  for (const SyncEntity& entity : entities) {
    UserEventSpecifics server_specifics = entity.specifics().user_event();
    // Find a matching event in our expectations. Same event time should mean
    // identical events, though there can be duplicates in some cases.
    auto iter = base::ranges::find(
        remaining_expected_specifics, server_specifics.event_time_usec(),
        &sync_pb::UserEventSpecifics::event_time_usec);
    // We don't expect to encounter id matching events with different values,
    // this isn't going to recover so fail the test case now.
    EXPECT_NE(iter, remaining_expected_specifics.end());
    if (remaining_expected_specifics.end() == iter) {
      return false;
    }
    // TODO(skym): This may need to change if we start updating navigation_id
    // based on what sessions data is committed, and end up committing the
    // same event multiple times.
    EXPECT_EQ(iter->navigation_id(), server_specifics.navigation_id());
    EXPECT_EQ(iter->event_case(), server_specifics.event_case());

    remaining_expected_specifics.erase(iter);
  }

  return true;
}
