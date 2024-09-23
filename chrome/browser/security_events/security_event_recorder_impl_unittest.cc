// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/security_events/security_event_recorder_impl.h"

#include <memory>

#include "base/time/default_clock.h"
#include "components/sync/protocol/security_event_specifics.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockSecurityEventSyncBridge : public SecurityEventSyncBridge {
 public:
  MockSecurityEventSyncBridge() = default;
  ~MockSecurityEventSyncBridge() override = default;

  MOCK_METHOD1(RecordSecurityEvent, void(sync_pb::SecurityEventSpecifics));
  MOCK_METHOD0(GetControllerDelegate,
               base::WeakPtr<syncer::DataTypeControllerDelegate>());
};

MATCHER_P(HasTimestampAndContainsGaiaPasswordReuse, event, "") {
  if (!arg.has_event_time_usec()) {
    return false;
  }

  if (arg.event_case() !=
      sync_pb::SecurityEventSpecifics::EventCase::kGaiaPasswordReuseEvent) {
    return false;
  }

  std::string actual_proto;
  std::string expected_proto;
  arg.gaia_password_reuse_event().SerializeToString(&actual_proto);
  event.SerializeToString(&expected_proto);
  return actual_proto == expected_proto;
}

}  // namespace

class SecurityEventRecorderImplTest : public testing::Test {};

TEST_F(SecurityEventRecorderImplTest, RecordGaiaPasswordReuse) {
  sync_pb::GaiaPasswordReuse expected_gaia_password_reuse_event;
  expected_gaia_password_reuse_event.mutable_reuse_detected()
      ->mutable_status()
      ->set_enabled(false);

  auto mock_security_event_sync_bridge =
      std::make_unique<MockSecurityEventSyncBridge>();
  EXPECT_CALL(*(mock_security_event_sync_bridge.get()),
              RecordSecurityEvent(HasTimestampAndContainsGaiaPasswordReuse(
                  expected_gaia_password_reuse_event)));

  sync_pb::GaiaPasswordReuse gaia_password_reuse_event;
  gaia_password_reuse_event.mutable_reuse_detected()
      ->mutable_status()
      ->set_enabled(false);

  auto security_event_recorder = std::make_unique<SecurityEventRecorderImpl>(
      std::move(mock_security_event_sync_bridge),
      base::DefaultClock::GetInstance());

  security_event_recorder->RecordGaiaPasswordReuse(gaia_password_reuse_event);
}
