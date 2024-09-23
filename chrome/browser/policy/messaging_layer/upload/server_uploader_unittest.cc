// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/server_uploader.h"

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
#include "base/types/expected.h"
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

class TestRecordHandler : public ServerUploader::RecordHandler {
 public:
  TestRecordHandler() = default;
  ~TestRecordHandler() override = default;

  void HandleRecords(
      bool need_encryption_key,
      int config_file_version,
      std::vector<EncryptedRecord> records,
      ScopedReservation scoped_reservation,
      UploadEnqueuedCallback enqueued_cb,
      CompletionCallback upload_complete,
      EncryptionKeyAttachedCallback encryption_key_attached_cb,
      ConfigFileAttachedCallback config_file_attached_cb) override {
    HandleRecords_(need_encryption_key, config_file_version, records,
                   std::move(scoped_reservation), std::move(enqueued_cb),
                   std::move(upload_complete),
                   std::move(encryption_key_attached_cb),
                   std::move(config_file_attached_cb));
  }

  MOCK_METHOD(void,
              HandleRecords_,
              (bool,
               int,
               std::vector<EncryptedRecord>&,
               ScopedReservation scoped_reservation,
               UploadEnqueuedCallback,
               CompletionCallback,
               EncryptionKeyAttachedCallback,
               ConfigFileAttachedCallback));
};

class ServerUploaderTestBase {
 public:
  ServerUploaderTestBase()
      : memory_resource_(base::MakeRefCounted<ResourceManager>(
            4u * 1024LLu * 1024LLu))  // 4 MiB
  {}

  virtual ~ServerUploaderTestBase() {
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

 protected:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});

  std::vector<EncryptedRecord> records_;

  const scoped_refptr<ResourceManager> memory_resource_;

  // Using the value -1 since it is the default value when creating the record
  // in `UploadEncryptedReportingRequestBuilder`.
  const int config_file_version_ = -1;
};

class ServerUploaderFailureTest : public ServerUploaderTestBase,
                                  public ::testing::TestWithParam<error::Code> {
};

class ServerUploaderTest : public ServerUploaderTestBase,
                           public ::testing::TestWithParam<
                               ::testing::tuple</*need_encryption_key*/ bool,
                                                /*force_confirm*/ bool>> {
 protected:
  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }
};

using TestSuccessfulUpload = MockFunction<void(SequenceInformation,
                                               /*force_confirm*/ bool)>;
using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;
using TestConfigFileAttached = MockFunction<void(ConfigFile)>;

TEST_P(ServerUploaderTest, ProcessesRecord) {
  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  const bool force_confirm_flag = force_confirm();
  auto handler = std::make_unique<TestRecordHandler>();
  EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<0, 4, 5, 6, 7>(Invoke(
          [&force_confirm_flag](
              bool need_encryption_key, UploadEnqueuedCallback enqueued_cb,
              CompletionCallback callback,
              EncryptionKeyAttachedCallback encryption_key_attached_cb,
              ConfigFileAttachedCallback config_file_attached_cb) {
            std::move(enqueued_cb).Run({});
            if (need_encryption_key) {
              std::move(encryption_key_attached_cb).Run(SignedEncryptionInfo());
            }
            std::move(config_file_attached_cb).Run(ConfigFile());
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

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(config_file_attached, Call(_)).Times(1);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_waiter;
  test::TestEvent<CompletionResponse> callback_waiter;
  Start<ServerUploader>(
      need_encryption_key(), config_file_version_, std::move(records_),
      std::move(record_reservation), std::move(handler), enqueued_waiter.cb(),
      std::move(successful_upload_cb), std::move(encryption_key_attached_cb),
      std::move(config_file_attached_cb), callback_waiter.cb(),
      sequenced_task_runner_);
  const auto& enqueued_result = enqueued_waiter.result();
  EXPECT_TRUE(enqueued_result.has_value());
  const auto response = callback_waiter.result();
  EXPECT_TRUE(response.has_value());
}

TEST_P(ServerUploaderTest, ProcessesRecords) {
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
  auto handler = std::make_unique<TestRecordHandler>();
  EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<0, 4, 5, 6, 7>(Invoke(
          [&force_confirm_flag](
              bool need_encryption_key, UploadEnqueuedCallback enqueued_cb,
              CompletionCallback callback,
              EncryptionKeyAttachedCallback encryption_key_attached_cb,
              ConfigFileAttachedCallback config_file_attached_cb) {
            std::move(enqueued_cb).Run({});
            if (need_encryption_key) {
              std::move(encryption_key_attached_cb).Run(SignedEncryptionInfo());
            }
            std::move(config_file_attached_cb).Run(ConfigFile());
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

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(config_file_attached, Call(_)).Times(1);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_waiter;
  test::TestEvent<CompletionResponse> callback_waiter;
  Start<ServerUploader>(
      need_encryption_key(), config_file_version_, std::move(records_),
      std::move(records_reservation), std::move(handler), enqueued_waiter.cb(),
      std::move(successful_upload_cb), std::move(encryption_key_attached_cb),
      std::move(config_file_attached_cb), callback_waiter.cb(),
      sequenced_task_runner_);
  const auto& enqueued_result = enqueued_waiter.result();
  EXPECT_TRUE(enqueued_result.has_value());
  const auto response = callback_waiter.result();
  EXPECT_TRUE(response.has_value());
}

TEST_P(ServerUploaderTest, ReportsFailureToProcess) {
  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  auto handler = std::make_unique<TestRecordHandler>();
  EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<4, 5>(Invoke([](UploadEnqueuedCallback enqueued_cb,
                                         CompletionCallback callback) {
        std::move(enqueued_cb)
            .Run(base::unexpected(
                Status(error::FAILED_PRECONDITION, "Fail for test")));
        std::move(callback).Run(base::unexpected(
            Status(error::FAILED_PRECONDITION, "Fail for test")));
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

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(config_file_attached, Call(_)).Times(0);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_waiter;
  test::TestEvent<CompletionResponse> callback_waiter;
  Start<ServerUploader>(
      need_encryption_key(), config_file_version_, std::move(records_),
      std::move(record_reservation), std::move(handler), enqueued_waiter.cb(),
      std::move(successful_upload_cb), std::move(encryption_key_attached_cb),
      std::move(config_file_attached_cb), callback_waiter.cb(),
      sequenced_task_runner_);
  const auto& enqueued_result = enqueued_waiter.result();
  EXPECT_THAT(enqueued_result.error(),
              Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
  const auto response = callback_waiter.result();
  EXPECT_THAT(response.error(),
              Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
}

TEST_P(ServerUploaderTest, ReportWithZeroRecords) {
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
  auto handler = std::make_unique<TestRecordHandler>();
  if (need_encryption_key()) {
    EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _))
        .WillOnce(WithArgs<0, 4, 5, 6>(Invoke(
            [&force_confirm_flag](
                bool need_encryption_key, UploadEnqueuedCallback enqueued_cb,
                CompletionCallback callback,
                EncryptionKeyAttachedCallback encryption_key_attached_cb) {
              std::move(enqueued_cb).Run({});
              if (need_encryption_key) {
                std::move(encryption_key_attached_cb)
                    .Run(SignedEncryptionInfo());
              }
              std::move(callback).Run(SuccessfulUploadResponse{
                  .force_confirm = force_confirm_flag});
            })));
  } else {
    EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _)).Times(0);
  }

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(config_file_attached, Call(_)).Times(0);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_waiter;
  Start<ServerUploader>(
      need_encryption_key(), config_file_version_, std::move(records_),
      std::move(no_records_reservation), std::move(handler),
      enqueued_waiter.cb(), std::move(successful_upload_cb),
      std::move(encryption_key_attached_cb), std::move(config_file_attached_cb),
      callback_waiter.cb(), sequenced_task_runner_);
  const auto& enqueued_result = enqueued_waiter.result();
  const auto response = callback_waiter.result();
  if (need_encryption_key()) {
    EXPECT_TRUE(enqueued_result.has_value());
    EXPECT_TRUE(response.has_value());
  } else {
    EXPECT_THAT(enqueued_result.error(),
                Property(&Status::error_code, Eq(error::NOT_FOUND)));
    EXPECT_THAT(response.error(),
                Property(&Status::error_code, Eq(error::INVALID_ARGUMENT)));
  }
}

TEST_P(ServerUploaderFailureTest, ReportsFailureToUpload) {
  const error::Code& error_code = GetParam();

  records_.emplace_back(DummyRecord());

  ScopedReservation record_reservation(records_.back().ByteSizeLong(),
                                       memory_resource_);
  EXPECT_TRUE(record_reservation.reserved());

  auto handler = std::make_unique<TestRecordHandler>();
  EXPECT_CALL(*handler, HandleRecords_(_, _, _, _, _, _, _, _))
      .WillOnce(WithArgs<4, 5>(Invoke([error_code](
                                          UploadEnqueuedCallback enqueued_cb,
                                          CompletionCallback callback) {
        std::move(enqueued_cb)
            .Run(base::unexpected(Status(error_code, "Fail for test")));
        std::move(callback).Run(
            base::unexpected(Status(error_code, "Failing for test")));
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

  StrictMock<TestConfigFileAttached> config_file_attached;
  EXPECT_CALL(config_file_attached, Call(_)).Times(0);
  auto config_file_attached_cb = base::BindRepeating(
      &TestConfigFileAttached::Call, base::Unretained(&config_file_attached));

  test::TestEvent<CompletionResponse> callback_waiter;
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_waiter;
  Start<ServerUploader>(
      /*need_encryption_key=*/true, config_file_version_, std::move(records_),
      std::move(record_reservation), std::move(handler), enqueued_waiter.cb(),
      std::move(successful_upload_cb), std::move(encryption_key_attached_cb),
      std::move(config_file_attached_cb), callback_waiter.cb(),
      sequenced_task_runner_);
  const auto& enqueued_result = enqueued_waiter.result();
  EXPECT_THAT(enqueued_result.error(),
              Property(&Status::error_code, Eq(error_code)));
  const auto response = callback_waiter.result();
  EXPECT_THAT(response.error(), Property(&Status::code, Eq(error_code)));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    ServerUploaderTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ServerUploaderFailureTests,
    ServerUploaderFailureTest,
    testing::Values(error::INVALID_ARGUMENT,
                    error::PERMISSION_DENIED,
                    error::OUT_OF_RANGE,
                    error::UNAVAILABLE),
    [](const testing::TestParamInfo<ServerUploaderFailureTest::ParamType>&
           info) { return Status(info.param, "").ToString(); });
}  // namespace
}  // namespace reporting
