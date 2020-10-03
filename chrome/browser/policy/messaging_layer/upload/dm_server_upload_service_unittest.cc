// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/browser/policy/messaging_layer/util/shared_vector.h"
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using testing::_;
using testing::Return;

// Usage (in tests only):
//
//   TestEvent<ResType> e;
//   ... Do some async work passing e.cb() as a completion callback of
//   base::OnceCallback<void(ResType* res)> type which also may perform some
//   other action specified by |done| callback provided by the caller.
//   ... = e.result();  // Will wait for e.cb() to be called and return the
//   collected result.
//
template <typename ResType>
class TestEvent {
 public:
  TestEvent() : run_loop_(std::make_unique<base::RunLoop>()) {}
  ~TestEvent() = default;
  TestEvent(const TestEvent& other) = delete;
  TestEvent& operator=(const TestEvent& other) = delete;
  ResType result() {
    run_loop_->Run();
    return std::forward<ResType>(result_);
  }

  // Completion callback to hand over to the processing method.
  base::OnceCallback<void(ResType res)> cb() {
    return base::BindOnce(
        [](base::RunLoop* run_loop, ResType* result, ResType res) {
          *result = std::forward<ResType>(res);
          run_loop->Quit();
        },
        base::Unretained(run_loop_.get()), base::Unretained(&result_));
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  ResType result_;
};

// Ensures that profile cannot be null.
TEST(DmServerUploadServiceTest, DeniesNullptrProfile) {
  content::BrowserTaskEnvironment task_envrionment;
  TestEvent<StatusOr<std::unique_ptr<DmServerUploadService>>> e;
  DmServerUploadService::Create(/*profile=*/nullptr, base::DoNothing(), e.cb());
  StatusOr<std::unique_ptr<DmServerUploadService>> result = e.result();
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.status().error_code(), error::INVALID_ARGUMENT);
}

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {}

  void CompleteExpectSuccess(
      DmServerUploadService::CompletionResponse response) {
    EXPECT_TRUE(response.ok());
    run_loop_->Quit();
  }

  void CompleteExpectUnimplemented(
      DmServerUploadService::CompletionResponse response) {
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::UNIMPLEMENTED);
    run_loop_->Quit();
  }

  void CompleteExpectInvalidArgument(
      DmServerUploadService::CompletionResponse response) {
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::INVALID_ARGUMENT);
    run_loop_->Quit();
  }

  void CompleteExpectFailedPrecondition(
      DmServerUploadService::CompletionResponse response) {
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::FAILED_PRECONDITION);
    run_loop_->Quit();
  }

  void CompleteExpectDeadlineExceeded(
      DmServerUploadService::CompletionResponse response) {
    EXPECT_FALSE(response.ok());
    EXPECT_EQ(response.status().error_code(), error::DEADLINE_EXCEEDED);
    run_loop_->Quit();
  }

  void Wait() { run_loop_->Run(); }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
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
  content::BrowserTaskEnvironment task_envrionment_{
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
