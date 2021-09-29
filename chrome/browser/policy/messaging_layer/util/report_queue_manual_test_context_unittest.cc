// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using testing::_;
using testing::Invoke;
using testing::WithArgs;

class ReportQueueManualTestContextTest : public testing::Test {
 protected:
  ReportQueueManualTestContextTest() = default;

  void SetUp() override {
    auto mock_report_queue = std::make_unique<reporting::MockReportQueue>();
    mock_report_queue_ = mock_report_queue.get();

    auto build_report_queue_cb = base::BindOnce(
        [](std::unique_ptr<ReportQueue> report_queue,
           std::unique_ptr<ReportQueueConfiguration> report_queue_config,
           base::OnceCallback<void(StatusOr<std::unique_ptr<ReportQueue>>)>
               report_queue_cb) {
          std::move(report_queue_cb).Run(std::move(report_queue));
        },
        std::move(mock_report_queue));

    ReportQueueManualTestContext::SetBuildReportQueueCallbackForTests(
        std::move(build_report_queue_cb));

    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("ABCDEF"));
  }

  const Priority priority_ = Priority::FAST_BATCH;
  const Destination destination_ = Destination::UPLOAD_EVENTS;
  reporting::MockReportQueue* mock_report_queue_;

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};
};

TEST_F(ReportQueueManualTestContextTest,
       BuildsReportQueueManualTestContextAndUploadsMessages) {
  uint64_t kNumberOfMessagesToEnqueue = 5;
  base::TimeDelta kFrequency = base::TimeDelta::FromSeconds(1);

  EXPECT_CALL(*mock_report_queue_, AddRecord(_, _, _))
      .Times(kNumberOfMessagesToEnqueue)
      .WillRepeatedly(
          WithArgs<2>(Invoke([](ReportQueue::EnqueueCallback enqueue_callback) {
            std::move(enqueue_callback).Run(Status::StatusOK());
          })));

  test::TestEvent<Status> completion_event;
  Start<ReportQueueManualTestContext>(
      kFrequency, kNumberOfMessagesToEnqueue, destination_, priority_,
      completion_event.cb(),
      base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits()));

  const Status status = completion_event.result();
  EXPECT_OK(status) << status;
}

}  // namespace
}  // namespace reporting
