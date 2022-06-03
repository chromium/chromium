// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/client/report_queue.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using testing::_;
using testing::Invoke;
using testing::WithArgs;

class ReportQueueManualTestContextTest : public testing::Test {
 protected:
  const Priority kPriority = Priority::FAST_BATCH;
  const Destination kDestination = Destination::UPLOAD_EVENTS;
  const uint64_t kNumberOfMessagesToEnqueue = 5;
  const base::TimeDelta kMessageFrequency = base::Seconds(1);

  void SetUp() override {
    task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits());
    mock_report_queue_ =
        std::unique_ptr<reporting::MockReportQueue, base::OnTaskRunnerDeleter>(
            new reporting::MockReportQueue(),
            base::OnTaskRunnerDeleter(task_runner_));
    auto build_report_queue_cb = base::BindOnce(
        &ReportQueueManualTestContextTest::BuildReportQueueCallback,
        base::Unretained(this));

    ReportQueueManualTestContext::SetBuildReportQueueCallbackForTests(
        std::move(build_report_queue_cb));

    policy::SetDMTokenForTesting(
        policy::DMToken::CreateValidTokenForTesting("ABCDEF"));
  }

  void BuildReportQueueCallback(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config,
      base::OnceCallback<void(
          StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>>)>
          report_queue_cb) {
    std::move(report_queue_cb).Run(std::move(mock_report_queue_));
    dm_token_ = std::move(report_queue_config)->dm_token();
  }

  std::unique_ptr<reporting::MockReportQueue, base::OnTaskRunnerDeleter>
      mock_report_queue_ = std::unique_ptr<reporting::MockReportQueue,
                                           base::OnTaskRunnerDeleter>(
          nullptr,
          base::OnTaskRunnerDeleter(nullptr));

  std::string dm_token_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ReportQueueManualTestContextTest,
       BuildsReportQueueManualTestContextAndUploadsDeviceEventMessages) {
  EXPECT_CALL(*mock_report_queue_, AddRecord(_, _, _))
      .Times(kNumberOfMessagesToEnqueue)
      .WillRepeatedly(
          WithArgs<2>(Invoke([](ReportQueue::EnqueueCallback enqueue_callback) {
            std::move(enqueue_callback).Run(Status::StatusOK());
          })));

  test::TestEvent<Status> completion_event;
  Start<ReportQueueManualTestContext>(
      kMessageFrequency, kNumberOfMessagesToEnqueue, kDestination, kPriority,
      completion_event.cb(), task_runner_);

  const Status status = completion_event.result();
  EXPECT_OK(status) << status;
  EXPECT_THAT(dm_token_, testing::IsEmpty());
}

}  // namespace
}  // namespace reporting
