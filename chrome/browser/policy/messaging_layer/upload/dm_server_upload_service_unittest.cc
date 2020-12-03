// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
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

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

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
  TestRecordHandler() : RecordHandler(/*client=*/nullptr) {}
  ~TestRecordHandler() override = default;

  void HandleRecords(
      std::unique_ptr<std::vector<EncryptedRecord>> records,
      DmServerUploadService::CompletionCallback upload_complete) override {
    HandleRecords_(records, upload_complete);
  }

  MOCK_METHOD(void,
              HandleRecords_,
              (std::unique_ptr<std::vector<EncryptedRecord>>&,
               DmServerUploadService::CompletionCallback&));
};

class DmServerUploaderTest : public testing::Test {
 public:
  DmServerUploaderTest()
      : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        handler_(std::make_unique<TestRecordHandler>()),
        records_(std::make_unique<std::vector<EncryptedRecord>>()) {}

 protected:
  content::BrowserTaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  std::unique_ptr<TestRecordHandler> handler_;
  std::unique_ptr<std::vector<EncryptedRecord>> records_;

  const base::TimeDelta kMaxDelay_ = base::TimeDelta::FromSeconds(1);
};

TEST_F(DmServerUploaderTest, ProcessesRecord) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecords_(_, _))
      .WillOnce(WithArgs<1>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(SequencingInformation());
          })));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectSuccess,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(std::move(records_),
                                                 handler_.get(), std::move(cb),
                                                 sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ProcessesRecords) {
  uint64_t kNumberOfRecords = 10;
  uint64_t kGenerationId = 1234;

  for (uint64_t i = 0; i < kNumberOfRecords; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(
        base::StrCat({"Record Number ", base::NumberToString(i)}));
    auto* sequencing_information =
        encrypted_record.mutable_sequencing_information();
    sequencing_information->set_generation_id(kGenerationId);
    sequencing_information->set_sequencing_id(i);
    sequencing_information->set_priority(Priority::IMMEDIATE);
    records_->push_back(std::move(encrypted_record));
  }

  EXPECT_CALL(*handler_, HandleRecords_(_, _))
      .WillOnce(WithArgs<1>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(SequencingInformation());
          })));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectSuccess,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(std::move(records_),
                                                 handler_.get(), std::move(cb),
                                                 sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ReportsFailureToProcess) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecords_(_, _))
      .WillOnce(WithArgs<1>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(
                Status(error::FAILED_PRECONDITION, "Fail for test"));
          })));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectFailedPrecondition,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(std::move(records_),
                                                 handler_.get(), std::move(cb),
                                                 sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, ReportsFailureToUpload) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecords_(_, _))
      .WillOnce(WithArgs<1>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(
                Status(error::DEADLINE_EXCEEDED, "Fail for test"));
          })));

  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectDeadlineExceeded,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(std::move(records_),
                                                 handler_.get(), std::move(cb),
                                                 sequenced_task_runner_);

  callback_waiter.Wait();
}

TEST_F(DmServerUploaderTest, FailWithZeroRecords) {
  TestCallbackWaiter callback_waiter;
  DmServerUploadService::CompletionCallback cb =
      base::BindOnce(&TestCallbackWaiter::CompleteExpectInvalidArgument,
                     base::Unretained(&callback_waiter));

  Start<DmServerUploadService::DmServerUploader>(std::move(records_),
                                                 handler_.get(), std::move(cb),
                                                 sequenced_task_runner_);

  callback_waiter.Wait();
}

}  // namespace
}  // namespace reporting
