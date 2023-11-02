// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/session_service_log.h"

#include "base/time/time.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

class SessionServiceLogTest : public testing::Test {
 public:
 protected:
  content::BrowserTaskEnvironment task_environment_;
  // |task_environment_| needs to still be alive when
  // |testing_profile_| is destroyed.
  TestingProfile testing_profile_;
};

TEST_F(SessionServiceLogTest, LogSessionServiceEvent) {
  SessionServiceEvent start_event;
  start_event.type = SessionServiceEventLogType::kStart;
  start_event.time = base::Time::Now();
  start_event.data.start.did_last_session_crash = true;
  LogSessionServiceEvent(&testing_profile_, start_event);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kStart, restored_event.type);
  EXPECT_EQ(start_event.time, restored_event.time);
  EXPECT_EQ(start_event.data.start.did_last_session_crash,
            restored_event.data.start.did_last_session_crash);
}

TEST_F(SessionServiceLogTest, LogSessionServiceStartEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceStartEvent(&testing_profile_, false);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kStart, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_FALSE(restored_event.data.start.did_last_session_crash);
}

TEST_F(SessionServiceLogTest, LogSessionServiceExitEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceExitEvent(&testing_profile_, 1, 2, true, false);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kExit, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(1, restored_event.data.exit.window_count);
  EXPECT_EQ(2, restored_event.data.exit.tab_count);
  EXPECT_TRUE(restored_event.data.exit.is_first_session_service);
  EXPECT_FALSE(restored_event.data.exit.did_schedule_command);
}

TEST_F(SessionServiceLogTest, LogSessionServiceExitEvent2) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceExitEvent(&testing_profile_, 1, 2, false, true);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kExit, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(1, restored_event.data.exit.window_count);
  EXPECT_EQ(2, restored_event.data.exit.tab_count);
  EXPECT_FALSE(restored_event.data.exit.is_first_session_service);
  EXPECT_TRUE(restored_event.data.exit.did_schedule_command);
}

TEST_F(SessionServiceLogTest, LogSessionServiceRestoreEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceRestoreEvent(&testing_profile_, 1, 2, true);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kRestore, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(1, restored_event.data.restore.window_count);
  EXPECT_EQ(2, restored_event.data.restore.tab_count);
  EXPECT_TRUE(restored_event.data.restore.encountered_error_reading);
}

TEST_F(SessionServiceLogTest, LogSessionServiceWriteErrorEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceWriteErrorEvent(&testing_profile_, false);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kWriteError, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(1, restored_event.data.write_error.error_count);
  EXPECT_EQ(0, restored_event.data.write_error.unrecoverable_error_count);
}

TEST_F(SessionServiceLogTest, LogSessionServiceUnrecoverableWriteErrorEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceWriteErrorEvent(&testing_profile_, true);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kWriteError, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(1, restored_event.data.write_error.error_count);
  EXPECT_EQ(1, restored_event.data.write_error.unrecoverable_error_count);
}

TEST_F(SessionServiceLogTest, LogSessionServiceRestoreCanceledEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceRestoreCanceledEvent(&testing_profile_);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kRestoreCanceled, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
}

TEST_F(SessionServiceLogTest, LogSessionServiceRestoreInitiatedEvent) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceRestoreInitiatedEvent(&testing_profile_,
                                         /*synchronous=*/true,
                                         /*restore_browser=*/false);
  LogSessionServiceRestoreInitiatedEvent(&testing_profile_,
                                         /*synchronous=*/false,
                                         /*restore_browser=*/true);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(2u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kRestoreInitiated, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_TRUE(restored_event.data.restore_initiated.synchronous);
  EXPECT_FALSE(restored_event.data.restore_initiated.restore_browser);

  auto restored_event2 = *(++events.begin());
  EXPECT_EQ(SessionServiceEventLogType::kRestoreInitiated,
            restored_event2.type);
  EXPECT_LE(start_time, restored_event2.time);
  EXPECT_FALSE(restored_event2.data.restore_initiated.synchronous);
  EXPECT_TRUE(restored_event2.data.restore_initiated.restore_browser);
}

TEST_F(SessionServiceLogTest, WriteErrorEventsCoalesce) {
  const base::Time start_time = base::Time::Now();
  LogSessionServiceWriteErrorEvent(&testing_profile_, false);
  LogSessionServiceWriteErrorEvent(&testing_profile_, true);
  LogSessionServiceWriteErrorEvent(&testing_profile_, true);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(1u, events.size());
  auto restored_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kWriteError, restored_event.type);
  EXPECT_LE(start_time, restored_event.time);
  EXPECT_EQ(3, restored_event.data.write_error.error_count);
  EXPECT_EQ(2, restored_event.data.write_error.unrecoverable_error_count);
}

TEST_F(SessionServiceLogTest, RemoveLastSessionServiceEventOfType) {
  LogSessionServiceExitEvent(&testing_profile_, 1, 2, true, true);
  LogSessionServiceWriteErrorEvent(&testing_profile_, false);
  LogSessionServiceExitEvent(&testing_profile_, 2, 3, true, true);
  LogSessionServiceWriteErrorEvent(&testing_profile_, false);
  RemoveLastSessionServiceEventOfType(&testing_profile_,
                                      SessionServiceEventLogType::kExit);
  auto events = GetSessionServiceEvents(&testing_profile_);
  ASSERT_EQ(3u, events.size());
  auto exit_event = *events.begin();
  EXPECT_EQ(SessionServiceEventLogType::kExit, exit_event.type);
  EXPECT_EQ(1, exit_event.data.exit.window_count);
  EXPECT_EQ(2, exit_event.data.exit.tab_count);
  auto iter = events.begin();
  ++iter;
  EXPECT_EQ(SessionServiceEventLogType::kWriteError, iter->type);
  ++iter;
  EXPECT_EQ(SessionServiceEventLogType::kWriteError, iter->type);
}
