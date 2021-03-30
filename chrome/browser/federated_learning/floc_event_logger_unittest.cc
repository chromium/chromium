// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_event_logger.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/federated_learning/floc_remote_permission_service.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/federated_learning/features/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_user_events/fake_user_event_service.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace federated_learning {

class FakeFlocRemotePermissionService : public FlocRemotePermissionService {
 public:
  using FlocRemotePermissionService::FlocRemotePermissionService;

  void QueryFlocPermission(QueryFlocPermissionCallback callback,
                           const net::PartialNetworkTrafficAnnotationTag&
                               partial_traffic_annotation) override {
    ++number_of_permission_queries_;
    std::move(callback).Run(swaa_nac_account_enabled_);
  }

  size_t number_of_permission_queries() const {
    return number_of_permission_queries_;
  }

  void set_swaa_nac_account_enabled(bool enabled) {
    swaa_nac_account_enabled_ = enabled;
  }

 private:
  size_t number_of_permission_queries_ = 0;
  bool swaa_nac_account_enabled_ = true;
};

class FlocEventLoggerUnitTest : public testing::Test {
 public:
  FlocEventLoggerUnitTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  ~FlocEventLoggerUnitTest() override = default;

  void SetUp() override {
    test_sync_service_ = std::make_unique<syncer::TestSyncService>();
    test_sync_service_->SetTransportState(
        syncer::SyncService::TransportState::DISABLED);

    fake_user_event_service_ = std::make_unique<syncer::FakeUserEventService>();

    fake_floc_remote_permission_service_ =
        std::make_unique<FakeFlocRemotePermissionService>(
            /*url_loader_factory=*/nullptr);

    floc_event_logger_ = std::make_unique<FlocEventLogger>(
        test_sync_service_.get(), fake_floc_remote_permission_service_.get(),
        fake_user_event_service_.get());
  }

  void DisableSyncHistory() {
    test_sync_service_->SetTransportState(
        syncer::SyncService::TransportState::DISABLED);
    test_sync_service_->FireStateChanged();
  }

  void EnableSyncHistory() {
    test_sync_service_->SetTransportState(
        syncer::SyncService::TransportState::ACTIVE);
    test_sync_service_->FireStateChanged();
  }

  void SetRemoteSwaaNacAccountEnabled(bool enabled) {
    fake_floc_remote_permission_service_->set_swaa_nac_account_enabled(enabled);
  }

  size_t GetNumberOfRemotePermissionQueries() {
    return fake_floc_remote_permission_service_->number_of_permission_queries();
  }

  size_t GetLoggedEventSize() const {
    return fake_user_event_service_->GetRecordedUserEvents().size();
  }

  FlocEventLogger::Event GetEventAtIndex(size_t i) {
    CHECK_LT(i, fake_user_event_service_->GetRecordedUserEvents().size());

    const sync_pb::UserEventSpecifics& specifics =
        fake_user_event_service_->GetRecordedUserEvents()[i];
    EXPECT_EQ(sync_pb::UserEventSpecifics::kFlocIdComputedEvent,
              specifics.event_case());
    const sync_pb::UserEventSpecifics_FlocIdComputed& e =
        specifics.floc_id_computed_event();

    return {
        e.has_floc_id(), e.has_floc_id() ? e.floc_id() : 0,
        base::Time::FromDeltaSinceWindowsEpoch(
            base::TimeDelta::FromMicroseconds(specifics.event_time_usec()))};
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<syncer::FakeUserEventService> fake_user_event_service_;
  std::unique_ptr<FakeFlocRemotePermissionService>
      fake_floc_remote_permission_service_;
  std::unique_ptr<FlocEventLogger> floc_event_logger_;

  DISALLOW_COPY_AND_ASSIGN(FlocEventLoggerUnitTest);
};

TEST_F(FlocEventLoggerUnitTest, DefaultSyncDisabled_EventLogging) {
  // Expect no loggings as sync is disabled.
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});
  EXPECT_EQ(0u, GetLoggedEventSize());

  // After 10 seconds, still no loggings.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(0u, GetLoggedEventSize());
}

TEST_F(FlocEventLoggerUnitTest, SyncEnabledWithinTenSeconds) {
  // Expect no loggings as sync is disabled.
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});
  EXPECT_EQ(0u, GetLoggedEventSize());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(9));
  EnableSyncHistory();
  EXPECT_EQ(0u, GetLoggedEventSize());

  // After 10 seconds, expect a logging as the previous logging is attempted for
  // the second time.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1u, GetLoggedEventSize());
  EXPECT_EQ(1u, GetNumberOfRemotePermissionQueries());
  EXPECT_EQ(true, GetEventAtIndex(0).sim_hash_computed);
  EXPECT_EQ(33u, GetEventAtIndex(0).sim_hash);
  EXPECT_EQ(base::Time::FromTimeT(44), GetEventAtIndex(0).time);
}

TEST_F(FlocEventLoggerUnitTest, SyncEnabledAfterTenSeconds) {
  // Expect no loggings as sync is disabled.
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});
  EXPECT_EQ(0u, GetLoggedEventSize());

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(11));

  // If sync is enabled after 10 seconds after the logging time, the event won't
  // be handled.
  EnableSyncHistory();
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10000));
  EXPECT_EQ(0u, GetLoggedEventSize());
}

TEST_F(FlocEventLoggerUnitTest, MultipleEventsBeforeSyncEnabled) {
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));

  floc_event_logger_->LogFlocComputedEvent(
      {false, 999, base::Time::FromTimeT(55)});

  EXPECT_EQ(0u, GetLoggedEventSize());

  EnableSyncHistory();

  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(5));

  // At time 10, the first event will be given its second attempt.
  EXPECT_EQ(1u, GetLoggedEventSize());
  EXPECT_EQ(1u, GetNumberOfRemotePermissionQueries());
  EXPECT_EQ(true, GetEventAtIndex(0).sim_hash_computed);
  EXPECT_EQ(33u, GetEventAtIndex(0).sim_hash);
  EXPECT_EQ(base::Time::FromTimeT(44), GetEventAtIndex(0).time);

  // At time 15, the second event will be given its second attempt.
  // The sim_hash field of the 2nd event (i.e. 999) was ignored because the
  // sim_hash_computed field is false.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(15));
  EXPECT_EQ(2u, GetLoggedEventSize());
  EXPECT_EQ(2u, GetNumberOfRemotePermissionQueries());
  EXPECT_EQ(false, GetEventAtIndex(1).sim_hash_computed);
  EXPECT_EQ(0u, GetEventAtIndex(1).sim_hash);
  EXPECT_EQ(base::Time::FromTimeT(55), GetEventAtIndex(1).time);
}

// Sync-enabled followed by event logging.
TEST_F(FlocEventLoggerUnitTest, SyncEnabled_EventLogging) {
  // When sync gets enabled first, following requests should succeed
  // immediately.
  EnableSyncHistory();
  EXPECT_EQ(0u, GetLoggedEventSize());

  // Log an event. Expect it to be logged immediately.
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});
  EXPECT_EQ(1u, GetLoggedEventSize());
  EXPECT_EQ(true, GetEventAtIndex(0).sim_hash_computed);
  EXPECT_EQ(33u, GetEventAtIndex(0).sim_hash);
  EXPECT_EQ(base::Time::FromTimeT(44), GetEventAtIndex(0).time);
}

// Sync is enabled but remote permission is disabled, the request should also
// fail immediately.
TEST_F(FlocEventLoggerUnitTest,
       SyncEnabled_RemotePermissionDisabled_EventLogging) {
  EnableSyncHistory();
  SetRemoteSwaaNacAccountEnabled(false);

  // Log an event. Expect it to fail immediately, as if failed the remote
  // permission check.
  floc_event_logger_->LogFlocComputedEvent(
      {true, 33, base::Time::FromTimeT(44)});
  EXPECT_EQ(0u, GetLoggedEventSize());

  SetRemoteSwaaNacAccountEnabled(true);
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(10));
  EXPECT_EQ(0u, GetLoggedEventSize());
}

}  // namespace federated_learning
