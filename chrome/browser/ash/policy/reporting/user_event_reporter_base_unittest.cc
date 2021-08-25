// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_base.h"

#include "base/memory/weak_ptr.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user_names.h"
#include "content/public/test/browser_task_environment.h"

namespace reporting {

class UserEventReporterBaseTester : public UserEventReporterBase {
 public:
  UserEventReporterBaseTester(
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue,
      ::reporting::Destination destination,
      base::WeakPtr<testing::StrictMock<::reporting::MockReportQueue>>
          mock_queue)
      : UserEventReporterBase(destination, std::move(report_queue)),
        mock_queue_(mock_queue) {}

  void ReportEvent(base::StringPiece record,
                   ::reporting::Priority priority) override {
    mock_queue_->Enqueue(record, priority,
                         base::BindOnce([](::reporting::Status status) {}));
  }

  base::WeakPtr<testing::StrictMock<::reporting::MockReportQueue>> mock_queue_;
};

class UserEventReporterBaseTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mock_queue_ = new testing::StrictMock<::reporting::MockReportQueue>();
    weak_mock_queue_factory_ = std::make_unique<base::WeakPtrFactory<
        testing::StrictMock<::reporting::MockReportQueue>>>(mock_queue_);
  }

  void TearDown() override { delete mock_queue_; }

  testing::StrictMock<::reporting::MockReportQueue>* mock_queue_;
  std::unique_ptr<
      base::WeakPtrFactory<testing::StrictMock<::reporting::MockReportQueue>>>
      weak_mock_queue_factory_;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(UserEventReporterBaseTest, TestReportEvent) {
  base::StringPiece dummy_record = "testrecord";
  auto dummy_queue =
      std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
  auto mock_queue = weak_mock_queue_factory_->GetWeakPtr();

  base::StringPiece record;
  ::reporting::Priority priority;
  EXPECT_CALL(*mock_queue, AddRecord)
      .WillOnce(
          [&record, &priority](base::StringPiece record_string,
                               ::reporting::Priority event_priority,
                               ::reporting::ReportQueue::EnqueueCallback) {
            record = record_string;
            priority = event_priority;
          });

  UserEventReporterBaseTester reporter(std::move(dummy_queue),
                                       ::reporting::Destination(0), mock_queue);

  reporter.ReportEvent(dummy_record, ::reporting::Priority::IMMEDIATE);
  EXPECT_EQ(priority, ::reporting::Priority::IMMEDIATE);
  EXPECT_EQ(dummy_record, record);
}
}  // namespace reporting
