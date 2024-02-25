// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include <string_view>

#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_testing_record.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

TEST(UserEventReporterHelperTest, TestReportEvent) {
  content::BrowserTaskEnvironment task_environment;

  UserEventReporterTestingRecord input_record;
  input_record.set_field1(100);

  auto mock_queue =
      std::unique_ptr<MockReportQueueStrict, base::OnTaskRunnerDeleter>(
          new MockReportQueueStrict(),
          base::OnTaskRunnerDeleter(
              base::SequencedTaskRunner::GetCurrentDefault()));

  UserEventReporterTestingRecord enqueued_record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce([&enqueued_record, &priority](std::string_view record_string,
                                              Priority event_priority,
                                              ReportQueue::EnqueueCallback) {
        EXPECT_TRUE(
            enqueued_record.ParseFromString(std::string(record_string)));
        priority = event_priority;
      });

  UserEventReporterHelper reporter(std::move(mock_queue));
  reporter.ReportEvent(
      std::make_unique<UserEventReporterTestingRecord>(input_record),
      Priority::IMMEDIATE);

  EXPECT_EQ(priority, Priority::IMMEDIATE);
  EXPECT_EQ(enqueued_record.field1(), input_record.field1());
  EXPECT_FALSE(enqueued_record.has_field2());
}

TEST(UserEventReporterHelperTest, TestReportEventWithCallback) {
  content::BrowserTaskEnvironment task_environment;

  int callback_run_count = 0;
  UserEventReporterTestingRecord input_record;
  input_record.set_field1(100);

  auto mock_queue =
      std::unique_ptr<MockReportQueueStrict, base::OnTaskRunnerDeleter>(
          new MockReportQueueStrict(),
          base::OnTaskRunnerDeleter(
              base::SequencedTaskRunner::GetCurrentDefault()));

  UserEventReporterTestingRecord enqueued_record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce([&enqueued_record, &priority](std::string_view record_string,
                                              Priority event_priority,
                                              ReportQueue::EnqueueCallback cb) {
        EXPECT_TRUE(
            enqueued_record.ParseFromString(std::string(record_string)));
        priority = event_priority;
        std::move(cb).Run(Status());
      });

  UserEventReporterHelper reporter(std::move(mock_queue));
  reporter.ReportEvent(
      std::make_unique<UserEventReporterTestingRecord>(input_record),
      Priority::IMMEDIATE,
      base::BindLambdaForTesting([&](Status) { ++callback_run_count; }));

  EXPECT_EQ(callback_run_count, 1);
  EXPECT_EQ(priority, Priority::IMMEDIATE);
  EXPECT_EQ(enqueued_record.field1(), input_record.field1());
  EXPECT_FALSE(enqueued_record.has_field2());
}
}  // namespace reporting
