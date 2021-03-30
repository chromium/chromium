// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "chrome/test/base/testing_profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/util/shared_vector.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

// Ensures that profile cannot be null.
TEST(DmServerUploadServiceTest, DeniesNullptrProfile) {
  content::BrowserTaskEnvironment task_envrionment;
  test::TestEvent<StatusOr<std::unique_ptr<DmServerUploadService>>> e;
  DmServerUploadService::Create(/*client=*/nullptr, base::DoNothing(),
                                base::DoNothing(), e.cb());
  StatusOr<std::unique_ptr<DmServerUploadService>> result = e.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
}

class TestRecordHandler : public DmServerUploadService::RecordHandler {
 public:
  TestRecordHandler() : RecordHandler(/*client=*/nullptr) {}
  ~TestRecordHandler() override = default;

  void HandleRecords(bool need_encryption_key,
                     std::unique_ptr<std::vector<EncryptedRecord>> records,
                     DmServerUploadService::CompletionCallback upload_complete,
                     DmServerUploadService::EncryptionKeyAttachedCallback
                         encryption_key_attached_cb) override {
    HandleRecords_(need_encryption_key, records, upload_complete,
                   encryption_key_attached_cb);
  }

  MOCK_METHOD(void,
              HandleRecords_,
              (bool,
               std::unique_ptr<std::vector<EncryptedRecord>>&,
               DmServerUploadService::CompletionCallback&,
               DmServerUploadService::EncryptionKeyAttachedCallback&));
};

class DmServerUploaderTest : public ::testing::TestWithParam<
                                 ::testing::tuple</*need_encryption_key*/ bool,
                                                  /*force_confirm*/ bool>> {
 public:
  DmServerUploaderTest()
      : sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner({})),
        handler_(std::make_unique<TestRecordHandler>()),
        records_(std::make_unique<std::vector<EncryptedRecord>>()) {}

 protected:
  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  std::unique_ptr<TestRecordHandler> handler_;
  std::unique_ptr<std::vector<EncryptedRecord>> records_;

  const base::TimeDelta kMaxDelay_ = base::TimeDelta::FromSeconds(1);
};

using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;

TEST_P(DmServerUploaderTest, ProcessesRecord) {
  // Add an empty record.
  records_->emplace_back();

  const bool force_confirm_flag = force_confirm();
  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _))
      .WillOnce(WithArgs<0, 2, 3>(
          Invoke([&force_confirm_flag](
                     bool need_encryption_key,
                     DmServerUploadService::CompletionCallback& callback,
                     DmServerUploadService::EncryptionKeyAttachedCallback&
                         encryption_key_attached_cb) {
            if (need_encryption_key) {
              encryption_key_attached_cb.Run(SignedEncryptionInfo());
            }
            std::move(callback).Run(
                DmServerUploadService::SuccessfulUploadResponse{
                    .force_confirm = force_confirm_flag});
          })));

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<DmServerUploadService::CompletionResponse> callback_waiter;
  Start<DmServerUploadService::DmServerUploader>(
      need_encryption_key(), std::move(records_), handler_.get(),
      callback_waiter.cb(), encryption_key_attached_cb, sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_OK(response);
}

TEST_P(DmServerUploaderTest, ProcessesRecords) {
  const int64_t kNumberOfRecords = 10;
  const int64_t kGenerationId = 1234;

  for (int64_t i = 0; i < kNumberOfRecords; i++) {
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

  const bool force_confirm_flag = force_confirm();
  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _))
      .WillOnce(WithArgs<0, 2, 3>(
          Invoke([&force_confirm_flag](
                     bool need_encryption_key,
                     DmServerUploadService::CompletionCallback& callback,
                     DmServerUploadService::EncryptionKeyAttachedCallback&
                         encryption_key_attached_cb) {
            if (need_encryption_key) {
              encryption_key_attached_cb.Run(SignedEncryptionInfo());
            }
            std::move(callback).Run(
                DmServerUploadService::SuccessfulUploadResponse{
                    .force_confirm = force_confirm_flag});
          })));

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<DmServerUploadService::CompletionResponse> callback_waiter;
  Start<DmServerUploadService::DmServerUploader>(
      need_encryption_key(), std::move(records_), handler_.get(),
      callback_waiter.cb(), encryption_key_attached_cb, sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_OK(response);
}

TEST_P(DmServerUploaderTest, ReportsFailureToProcess) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _))
      .WillOnce(WithArgs<2>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(
                Status(error::FAILED_PRECONDITION, "Fail for test"));
          })));

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<DmServerUploadService::CompletionResponse> callback_waiter;
  Start<DmServerUploadService::DmServerUploader>(
      need_encryption_key(), std::move(records_), handler_.get(),
      callback_waiter.cb(), encryption_key_attached_cb, sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_THAT(response.status(),
              Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
}

TEST_P(DmServerUploaderTest, ReportsFailureToUpload) {
  // Add an empty record.
  records_->emplace_back();

  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _))
      .WillOnce(WithArgs<2>(
          Invoke([](DmServerUploadService::CompletionCallback& callback) {
            std::move(callback).Run(
                Status(error::DEADLINE_EXCEEDED, "Fail for test"));
          })));

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<DmServerUploadService::CompletionResponse> callback_waiter;
  Start<DmServerUploadService::DmServerUploader>(
      need_encryption_key(), std::move(records_), handler_.get(),
      callback_waiter.cb(), encryption_key_attached_cb, sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_THAT(response.status(),
              Property(&Status::error_code, Eq(error::DEADLINE_EXCEEDED)));
}

TEST_P(DmServerUploaderTest, ReprotWithZeroRecords) {
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  const bool force_confirm_flag = force_confirm();
  if (need_encryption_key()) {
    EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _))
        .WillOnce(WithArgs<0, 2, 3>(
            Invoke([&force_confirm_flag](
                       bool need_encryption_key,
                       DmServerUploadService::CompletionCallback& callback,
                       DmServerUploadService::EncryptionKeyAttachedCallback&
                           encryption_key_attached_cb) {
              if (need_encryption_key) {
                encryption_key_attached_cb.Run(SignedEncryptionInfo());
              }
              std::move(callback).Run(
                  DmServerUploadService::SuccessfulUploadResponse{
                      .force_confirm = force_confirm_flag});
            })));
  } else {
    EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _)).Times(0);
  }

  test::TestEvent<DmServerUploadService::CompletionResponse> callback_waiter;
  Start<DmServerUploadService::DmServerUploader>(
      need_encryption_key(), std::move(records_), handler_.get(),
      callback_waiter.cb(), encryption_key_attached_cb, sequenced_task_runner_);

  const auto response = callback_waiter.result();
  if (need_encryption_key()) {
    EXPECT_OK(response);
  } else {
    EXPECT_THAT(response.status(),
                Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
  }
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    DmServerUploaderTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
