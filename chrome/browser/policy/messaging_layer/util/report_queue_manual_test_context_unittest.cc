// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/util/report_queue_manual_test_context.h"

#include "base/functional/bind.h"
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

using ::testing::_;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::WithArgs;

namespace reporting {
namespace {

class ReportQueueManualTestContextTest : public testing::Test {
 protected:
  const Priority kPriority = Priority::FAST_BATCH;
  const Destination kDestination = Destination::UPLOAD_EVENTS;
  const uint64_t kNumberOfMessagesToEnqueue = 5;
  const base::TimeDelta kMessagePeriod = base::Seconds(1);

  void SetUp() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
    mock_report_queue_ =
        std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
            new MockReportQueue(), base::OnTaskRunnerDeleter(task_runner_));

    policy::SetDMTokenForTesting(policy::DMToken::CreateValidToken("ABCDEF"));
  }

  StatusOr<std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>> BuildQueue(
      std::unique_ptr<ReportQueueConfiguration> report_queue_config) {
    EXPECT_TRUE(mock_report_queue_) << "Can be only called once";
    dm_token_ = report_queue_config->dm_token();
    return std::move(mock_report_queue_);
  }

  ReportQueueManualTestContext::BuildReportQueueCallback GetQueueBuilder() {
    return base::BindOnce(&ReportQueueManualTestContextTest::BuildQueue,
                          base::Unretained(this));
  }

  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>
      mock_report_queue_ =
          std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
              nullptr,
              base::OnTaskRunnerDeleter(nullptr));

  std::string dm_token_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

// TODO(crbug.com/40878091) Disabled due to flake on Mac.
#if BUILDFLAG(IS_MAC)
TEST_F(
    ReportQueueManualTestContextTest,
    DISABLED_BuildsReportQueueManualTestContextAndUploadsDeviceEventMessages) {
#else
TEST_F(ReportQueueManualTestContextTest,
       BuildsReportQueueManualTestContextAndUploadsDeviceEventMessages) {
#endif
  EXPECT_CALL(*mock_report_queue_, AddRecord(_, _, _))
      .Times(kNumberOfMessagesToEnqueue)
      .WillRepeatedly(
          WithArgs<2>(Invoke([](ReportQueue::EnqueueCallback enqueue_callback) {
            std::move(enqueue_callback).Run(Status::StatusOK());
          })));

  test::TestEvent<Status> completion_event;
  Start<ReportQueueManualTestContext>(
      kMessagePeriod, kNumberOfMessagesToEnqueue, kDestination, kPriority,
      EventType::kDevice, completion_event.cb(), task_runner_,
      GetQueueBuilder());

  const Status status = completion_event.result();
  EXPECT_OK(status) << status;
  EXPECT_THAT(dm_token_, IsEmpty());
}

#if BUILDFLAG(IS_MAC)
TEST_F(ReportQueueManualTestContextTest,
       DISABLED_BuildsReportQueueManualTestContextAndUploadsUserEventMessages) {
#else
TEST_F(ReportQueueManualTestContextTest,
       BuildsReportQueueManualTestContextAndUploadsUserEventMessages) {
#endif
  EXPECT_CALL(*mock_report_queue_, AddRecord(_, _, _))
      .Times(kNumberOfMessagesToEnqueue)
      .WillRepeatedly(
          WithArgs<2>(Invoke([](ReportQueue::EnqueueCallback enqueue_callback) {
            std::move(enqueue_callback).Run(Status::StatusOK());
          })));

  test::TestEvent<Status> completion_event;
  Start<ReportQueueManualTestContext>(
      kMessagePeriod, kNumberOfMessagesToEnqueue, kDestination, kPriority,
      EventType::kUser, completion_event.cb(), task_runner_, GetQueueBuilder());

  const Status status = completion_event.result();
  EXPECT_OK(status) << status;
  EXPECT_THAT(dm_token_, IsEmpty());
}

}  // namespace
}  // namespace reporting
