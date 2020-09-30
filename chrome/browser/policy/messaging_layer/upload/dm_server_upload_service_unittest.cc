// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/util/shared_vector.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using testing::_;
using testing::Return;

// Ensures that profile cannot be null.
TEST(DmServerUploadServiceTest, DeniesNullptrProfile) {
  auto result =
      DmServerUploadService::Create(/*profile=*/nullptr, base::DoNothing());
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().error_code(), error::INVALID_ARGUMENT);
}

class TestCallbackWaiter {
 public:
  TestCallbackWaiter()
      : completed_(base::WaitableEvent::ResetPolicy::MANUAL,
                   base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  void CompleteExpectSuccess(
      DmServerUploadService::CompletionResponse response) {
    DCHECK(!completed_.IsSignaled());
    EXPECT_TRUE(response.ok());
    completed_.Signal();
  }

  void CompleteExpectUnimplemented(
      DmServerUploadService::CompletionResponse response) {
    DCHECK(!completed_.IsSignaled());
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::UNIMPLEMENTED);
    completed_.Signal();
  }

  void CompleteExpectInvalidArgument(
      DmServerUploadService::CompletionResponse response) {
    DCHECK(!completed_.IsSignaled());
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::INVALID_ARGUMENT);
    completed_.Signal();
  }

  void CompleteExpectFailedPrecondition(
      DmServerUploadService::CompletionResponse response) {
    DCHECK(!completed_.IsSignaled());
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::FAILED_PRECONDITION);
    completed_.Signal();
  }

  void CompleteExpectDeadlineExceeded(
      DmServerUploadService::CompletionResponse response) {
    DCHECK(!completed_.IsSignaled());
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::DEADLINE_EXCEEDED);
    completed_.Signal();
  }

  void Wait() { completed_.Wait(); }

 private:
  base::WaitableEvent completed_;
};

class TestRecordHandler : public DmServerUploadService::RecordHandler {
 public:
  explicit TestRecordHandler(policy::CloudPolicyClient* client)
      : RecordHandler(client) {}

  ~TestRecordHandler() override = default;

  MOCK_METHOD(Status, HandleRecord, (Record));
};

class DmServerUploaderTest : public testing::Test {
 public:
  DmServerUploaderTest()
      : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        handlers_(SharedVector<std::unique_ptr<
                      DmServerUploadService::RecordHandler>>::Create()) {}

  void SetUp() override {
    std::unique_ptr<TestRecordHandler> handler_ptr(
        new TestRecordHandler(&client_));
    handler_ = handler_ptr.get();
    handlers_->PushBack(std::move(handler_ptr), base::DoNothing());
    records_ = std::make_unique<std::vector<EncryptedRecord>>();
  }

 protected:
  base::test::TaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  TestRecordHandler* handler_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  scoped_refptr<
      SharedVector<std::unique_ptr<DmServerUploadService::RecordHandler>>>
      handlers_;

  std::unique_ptr<std::vector<EncryptedRecord>> records_;

  const base::TimeDelta kMaxDelay_ = base::TimeDelta::FromSeconds(1);

 private:
  policy::MockCloudPolicyClient client_;
};

TEST_F(DmServerUploaderTest, ProcessesRecord) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecord(_)).WillOnce(Return(Status::StatusOK()));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectSuccess,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ProcessesRecords) {
  for (uint64_t i = 0; i < 10; i++) {
    EncryptedRecord record;
    auto* sequencing_info = record.mutable_sequencing_information();
    sequencing_info->set_sequencing_id(i);

    records_->push_back(record);
  }

  EXPECT_CALL(*handler_, HandleRecord(_))
      .Times(10)
      .WillRepeatedly(Return(Status::StatusOK()));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectSuccess,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, DeniesBadWrappedRecord) {
  EncryptedRecord record;
  record.set_encrypted_wrapped_record("El Chupacabra");
  records_->push_back(record);

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectInvalidArgument,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ReportsFailureToProcess) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecord(_))
      .WillOnce(Return(Status(error::INVALID_ARGUMENT, "Fail for test")));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectFailedPrecondition,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ReportsFailureToUpload) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecord(_))
      .WillRepeatedly(
          Return(Status(error::DEADLINE_EXCEEDED, "Fail for test")));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectFailedPrecondition,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, FailWithZeroRecords) {
  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectInvalidArgument,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(
      std::move(records_), handlers_, std::move(cb), sequenced_task_runner_);

  callback_waiter.Wait();
}

}  // namespace
}  // namespace reporting
