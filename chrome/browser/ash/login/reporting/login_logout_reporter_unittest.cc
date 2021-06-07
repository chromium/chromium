// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/reporting/login_logout_reporter_test_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/test/task_environment.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"

namespace chromeos {
namespace reporting {

TEST(LoginLogoutReporterTest, ReportAffiliatedLogin) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ true,
      policy::DMToken::CreateValidTokenForTesting("token"),
      std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogin(user_email);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp());
  EXPECT_FALSE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST(LoginLogoutReporterTest, ReportUnaffiliatedLogin) {
  static constexpr char user_email[] = "unaffiliated@unmanaged.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ false,
      policy::DMToken::CreateValidTokenForTesting("token"),
      std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogin(user_email);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp());
  EXPECT_FALSE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
  ASSERT_TRUE(record.has_login_event());
  EXPECT_FALSE(record.login_event().has_failure());
}

TEST(LoginLogoutReporterTest, ReportAffiliatedLogout) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ true,
      policy::DMToken::CreateValidTokenForTesting("token"),
      std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogout(user_email);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  ASSERT_TRUE(record.has_affiliated_user());
  ASSERT_TRUE(record.affiliated_user().has_user_email());
  EXPECT_THAT(record.affiliated_user().user_email(), testing::Eq(user_email));
}

TEST(LoginLogoutReporterTest, ReportUnaffiliatedLogout) {
  static constexpr char user_email[] = "unaffiliated@unmanaged.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  LoginLogoutRecord record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record.ParseFromString(std::string(record_string));
            priority = event_priority;
          });

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ false,
      policy::DMToken::CreateValidTokenForTesting("token"),
      std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogout(user_email);

  EXPECT_THAT(priority, testing::Eq(::reporting::Priority::IMMEDIATE));
  EXPECT_TRUE(record.has_event_timestamp());
  EXPECT_FALSE(record.has_login_event());
  EXPECT_TRUE(record.has_logout_event());
  EXPECT_FALSE(record.has_affiliated_user());
}

TEST(LoginLogoutReporterTest, InvalidDMToken) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ true,
      policy::DMToken::CreateInvalidTokenForTesting(), std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogout(user_email);
}

TEST(LoginLogoutReporterTest, EmptyDMToken) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ true,
      /* should_report_user= */ true,
      policy::DMToken::CreateEmptyTokenForTesting(), std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogout(user_email);
}

TEST(LoginLogoutReporterTest, ShouldNotReportEvent) {
  static constexpr char user_email[] = "affiliated@managed.org";
  auto mock_queue =
      std::make_unique<testing::StrictMock<::reporting::MockReportQueue>>();

  EXPECT_CALL(*mock_queue, AddRecord).Times(0);

  auto test_delegate = std::make_unique<LoginLogoutReporterTestDelegate>(
      /* should_report_event= */ false,
      /* should_report_user= */ true,
      policy::DMToken::CreateValidTokenForTesting("token"),
      std::move(mock_queue));

  LoginLogoutReporter reporter(std::move(test_delegate));
  reporter.MaybeReportLogout(user_email);
}
}  // namespace reporting
}  // namespace chromeos
