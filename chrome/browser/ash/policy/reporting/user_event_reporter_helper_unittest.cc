// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/reporting/user_event_reporter_testing_record.pb.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {

TEST(UserEventReporterHelperTest, TestReportEvent) {
  content::BrowserTaskEnvironment task_environment;

  UserEventReporterTestingRecord input_record;
  input_record.set_field1(100);

  auto mock_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new testing::StrictMock<MockReportQueue>(),
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));

  UserEventReporterTestingRecord enqueued_record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce([&enqueued_record, &priority](base::StringPiece record_string,
                                              Priority event_priority,
                                              ReportQueue::EnqueueCallback) {
        EXPECT_TRUE(
            enqueued_record.ParseFromString(std::string(record_string)));
        priority = event_priority;
      });

  UserEventReporterHelper reporter(std::move(mock_queue));
  reporter.ReportEvent(&input_record, Priority::IMMEDIATE);

  EXPECT_EQ(priority, Priority::IMMEDIATE);
  EXPECT_EQ(enqueued_record.field1(), input_record.field1());
  EXPECT_FALSE(enqueued_record.has_field2());
}
}  // namespace reporting
