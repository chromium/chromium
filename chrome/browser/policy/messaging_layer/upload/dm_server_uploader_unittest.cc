// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"

#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace reporting {
namespace {

EncryptedRecord DummyRecord() {
  EncryptedRecord record;
  record.set_encrypted_wrapped_record("TEST_DATA");
  return record;
}

class TestRecordHandler : public RecordHandler {
 public:
  TestRecordHandler() = default;
  ~TestRecordHandler() override = default;

  void HandleRecords(
      bool need_encryption_key,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      CompletionCallback upload_complete,
      EncryptionKeyAttachedCallback encryption_key_attached_cb) override {
    HandleRecords_(need_encryption_key, records, std::move(scoped_reservation),
                   std::move(upload_complete),
                   std::move(encryption_key_attached_cb));
  }

  MOCK_METHOD(void,
              HandleRecords_,
              (bool,
               std::vector<EncryptedRecord>&,
               ScopedReservation scoped_reservation,
               CompletionCallback,
               EncryptionKeyAttachedCallback));
};

class DmServerTest {
 public:
  DmServerTest()
      : handler_(std::make_unique<TestRecordHandler>()),
        memory_resource_(base::MakeRefCounted<ResourceManager>(
            4u * 1024LLu * 1024LLu))  // 4 MiB
  {}

  virtual ~DmServerTest() { EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL)); }

 protected:
  content::BrowserTaskEnvironment task_envrionment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});
  const std::unique_ptr<TestRecordHandler> handler_;

  std::vector<EncryptedRecord> records_;

  const scoped_refptr<ResourceManager> memory_resource_;
};

class DmServerFailureTest : public DmServerTest,
                            public testing::TestWithParam<error::Code> {};

class DmServerUploaderTest : public DmServerTest,
                             public testing::TestWithParam<
                                 ::testing::tuple</*need_encryption_key*/ bool,
                                                  /*force_confirm*/ bool>> {
 protected:
  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }
};

using TestSuccessfulUpload = MockFunction<void(SequenceInformation,
                                               /*force_confirm*/ bool)>;
using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;

TEST_P(DmServerUploaderTest, ProcessesRecord) {
  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  const bool force_confirm_flag = force_confirm();
  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _))
      .WillOnce(WithArgs<0, 3, 4>(
          Invoke([&force_confirm_flag](
                     bool need_encryption_key, CompletionCallback callback,
                     EncryptionKeyAttachedCallback encryption_key_attached_cb) {
            if (need_encryption_key) {
              std::move(encryption_key_attached_cb).Run(SignedEncryptionInfo());
            }
            std::move(callback).Run(
                SuccessfulUploadResponse{.force_confirm = force_confirm_flag});
          })));

  StrictMock<TestSuccessfulUpload> successful_upload;
  EXPECT_CALL(successful_upload, Call(_, _)).Times(1);
  auto successful_upload_cb = base::BindRepeating(
      &TestSuccessfulUpload::Call, base::Unretained(&successful_upload));
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  Start<DmServerUploader>(need_encryption_key(), std::move(records_),
                          std::move(record_reservation), handler_.get(),
                          std::move(successful_upload_cb),
                          std::move(encryption_key_attached_cb),
                          callback_waiter.cb(), sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_OK(response);
}

TEST_P(DmServerUploaderTest, ProcessesRecords) {
  const int64_t kNumberOfRecords = 10;
  const int64_t kGenerationId = 1234;

  ScopedReservation records_reservation(0uL, memory_resource_);
  EXPECT_FALSE(records_reservation.reserved());
  for (int64_t i = 0; i < kNumberOfRecords; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(
        base::StrCat({"Record Number ", base::NumberToString(i)}));
    auto* sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_generation_id(kGenerationId);
    sequence_information->set_sequencing_id(i);
    sequence_information->set_priority(Priority::IMMEDIATE);
    ScopedReservation record_reservation(encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    EXPECT_TRUE(record_reservation.reserved());
    records_reservation.HandOver(record_reservation);
    records_.push_back(std::move(encrypted_record));
  }
  EXPECT_TRUE(records_reservation.reserved());

  const bool force_confirm_flag = force_confirm();
  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _))
      .WillOnce(WithArgs<0, 3, 4>(
          Invoke([&force_confirm_flag](
                     bool need_encryption_key, CompletionCallback callback,
                     EncryptionKeyAttachedCallback encryption_key_attached_cb) {
            if (need_encryption_key) {
              std::move(encryption_key_attached_cb).Run(SignedEncryptionInfo());
            }
            std::move(callback).Run(
                SuccessfulUploadResponse{.force_confirm = force_confirm_flag});
          })));

  StrictMock<TestSuccessfulUpload> successful_upload;
  EXPECT_CALL(successful_upload, Call(_, _)).Times(1);
  auto successful_upload_cb = base::BindRepeating(
      &TestSuccessfulUpload::Call, base::Unretained(&successful_upload));
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  Start<DmServerUploader>(need_encryption_key(), std::move(records_),
                          std::move(records_reservation), handler_.get(),
                          std::move(successful_upload_cb),
                          std::move(encryption_key_attached_cb),
                          callback_waiter.cb(), sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_OK(response);
}

TEST_P(DmServerUploaderTest, ReportsFailureToProcess) {
  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _))
      .WillOnce(WithArgs<3>(Invoke([](CompletionCallback callback) {
        std::move(callback).Run(
            Status(error::FAILED_PRECONDITION, "Fail for test"));
      })));

  StrictMock<TestSuccessfulUpload> successful_upload;
  EXPECT_CALL(successful_upload, Call(_, _)).Times(0);
  auto successful_upload_cb = base::BindRepeating(
      &TestSuccessfulUpload::Call, base::Unretained(&successful_upload));
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  Start<DmServerUploader>(need_encryption_key(), std::move(records_),
                          std::move(record_reservation), handler_.get(),
                          std::move(successful_upload_cb),
                          std::move(encryption_key_attached_cb),
                          callback_waiter.cb(), sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_THAT(response.status(),
              Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
}

TEST_P(DmServerUploaderTest, ReprotWithZeroRecords) {
  ScopedReservation no_records_reservation(0uL, memory_resource_);
  EXPECT_FALSE(no_records_reservation.reserved());

  StrictMock<TestSuccessfulUpload> successful_upload;
  EXPECT_CALL(successful_upload, Call(_, _))
      .Times(need_encryption_key() ? 1 : 0);
  auto successful_upload_cb = base::BindRepeating(
      &TestSuccessfulUpload::Call, base::Unretained(&successful_upload));
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  const bool force_confirm_flag = force_confirm();
  if (need_encryption_key()) {
    EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _))
        .WillOnce(WithArgs<0, 3, 4>(Invoke(
            [&force_confirm_flag](
                bool need_encryption_key, CompletionCallback callback,
                EncryptionKeyAttachedCallback encryption_key_attached_cb) {
              if (need_encryption_key) {
                std::move(encryption_key_attached_cb)
                    .Run(SignedEncryptionInfo());
              }
              std::move(callback).Run(SuccessfulUploadResponse{
                  .force_confirm = force_confirm_flag});
            })));
  } else {
    EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _)).Times(0);
  }

  test::TestEvent<CompletionResponse> callback_waiter;
  Start<DmServerUploader>(need_encryption_key(), std::move(records_),
                          std::move(no_records_reservation), handler_.get(),
                          std::move(successful_upload_cb),
                          std::move(encryption_key_attached_cb),
                          callback_waiter.cb(), sequenced_task_runner_);

  const auto response = callback_waiter.result();
  if (need_encryption_key()) {
    EXPECT_OK(response);
  } else {
    EXPECT_THAT(response.status(),
                Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
  }
}

TEST_P(DmServerFailureTest, ReportsFailureToUpload) {
  const error::Code& error_code = GetParam();

  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  EXPECT_CALL(*handler_, HandleRecords_(_, _, _, _, _))
      .WillOnce(WithArgs<3>(Invoke([error_code](CompletionCallback callback) {
        std::move(callback).Run(Status(error_code, "Failing for test"));
      })));

  StrictMock<TestSuccessfulUpload> successful_upload;
  EXPECT_CALL(successful_upload, Call(_, _)).Times(0);
  auto successful_upload_cb = base::BindRepeating(
      &TestSuccessfulUpload::Call, base::Unretained(&successful_upload));
  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);
  auto encryption_key_attached_cb =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  Start<DmServerUploader>(
      /*need_encryption_key=*/true, std::move(records_),
      std::move(record_reservation), handler_.get(),
      std::move(successful_upload_cb), std::move(encryption_key_attached_cb),
      callback_waiter.cb(), sequenced_task_runner_);

  const auto response = callback_waiter.result();
  EXPECT_THAT(response.status(), Property(&Status::code, Eq(error_code)));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    DmServerUploaderTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    DmServerFailureTests,
    DmServerFailureTest,
    testing::Values(error::INVALID_ARGUMENT,
                    error::PERMISSION_DENIED,
                    error::OUT_OF_RANGE,
                    error::UNAVAILABLE),
    [](const testing::TestParamInfo<DmServerFailureTest::ParamType>& info) {
      return Status(info.param, "").ToString();
    });
}  // namespace
}  // namespace reporting
