// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Ge;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Lt;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::StrEq;
using ::testing::WithArgs;

using ::ash::reporting::LogUploadEvent;

namespace reporting {
namespace {

MATCHER_P(ResponseEquals,
          expected,
          "Compares StatusOr<response> to expected response") {
  if (!arg.ok()) {
    return false;
  }
  if (arg.ValueOrDie().sequence_information.GetTypeName() !=
      expected.sequence_information.GetTypeName()) {
    return false;
  }
  if (arg.ValueOrDie().sequence_information.SerializeAsString() !=
      expected.sequence_information.SerializeAsString()) {
    return false;
  }
  return arg.ValueOrDie().force_confirm == expected.force_confirm;
}

class MockFileUploadDelegate : public FileUploadJob::Delegate {
 public:
  MOCK_METHOD(void,
              DoInitiate,
              (base::StringPiece origin_path,
               base::StringPiece upload_parameters,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*total*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(void,
              DoNextStep,
              (int64_t total,
               int64_t uploaded,
               base::StringPiece session_token,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*uploaded*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(
      void,
      DoFinalize,
      (base::StringPiece session_token,
       base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
           cb),
      (override));
};

// Tests for generic events handling.
class RecordHandlerUploadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
  }

  void TearDown() override {
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner({});

  FileUploadJob::TestEnvironment manager_test_env_;
  ReportingServerConnector::TestEnvironment test_env_;

  scoped_refptr<ResourceManager> memory_resource_;
};

EncryptedRecord ComposeEncryptedRecord(
    base::StringPiece data,
    UploadSettings upload_settings,
    absl::optional<UploadTracker> upload_tracker) {
  static constexpr int64_t kGenerationId = 1234;
  EncryptedRecord encrypted_record;
  encrypted_record.set_encrypted_wrapped_record(data.data(), data.size());
  auto* sequence_information = encrypted_record.mutable_sequence_information();
  sequence_information->set_generation_id(kGenerationId);
  sequence_information->set_sequencing_id(0);
  sequence_information->set_priority(Priority::IMMEDIATE);
  {
    auto* const record_copy = encrypted_record.mutable_record_copy();
    record_copy->set_destination(Destination::LOG_UPLOAD);
    ash::reporting::LogUploadEvent log_upload_event;
    *log_upload_event.mutable_upload_settings() = std::move(upload_settings);
    if (upload_tracker.has_value()) {
      *log_upload_event.mutable_upload_tracker() =
          std::move(upload_tracker.value());
    }
    EXPECT_TRUE(
        log_upload_event.SerializeToString(record_copy->mutable_data()));
  }
  return encrypted_record;
}

UploadSettings ComposeUploadSettings(int64_t retry_count = 0) {
  UploadSettings settings;
  settings.set_origin_path("/tmp/file");
  settings.set_retry_count(retry_count);
  settings.set_upload_parameters("http://upload");
  return settings;
}

UploadTracker ComposeUploadTracker(int64_t total, int64_t uploaded) {
  UploadTracker tracker;
  tracker.set_total(total);
  tracker.set_uploaded(uploaded);
  tracker.set_session_token("ABC");
  return tracker;
}

UploadTracker ComposeDoneTracker(int64_t total) {
  UploadTracker tracker;
  tracker.set_total(total);
  tracker.set_uploaded(tracker.total());
  tracker.set_access_parameters("http://destination");
  return tracker;
}

::testing::Matcher<LogUploadEvent> MatchSettings() {
  return Property(
      &LogUploadEvent::upload_settings,
      AllOf(Property(&UploadSettings::retry_count, Eq(0)),
            Property(&UploadSettings::origin_path, StrEq("/tmp/file")),
            Property(&UploadSettings::upload_parameters,
                     StrEq("http://upload"))));
}

::testing::Matcher<LogUploadEvent> MatchTrackerInProgress(
    int64_t uploaded,
    int64_t total,
    base::StringPiece session_token) {
  return Property(
      &LogUploadEvent::upload_tracker,
      AllOf(Property(&UploadTracker::uploaded, Eq(uploaded)),
            Property(&UploadTracker::total, Eq(total)),
            Property(&UploadTracker::session_token, StrEq(session_token)),
            Property(&UploadTracker::access_parameters, IsEmpty())));
}

::testing::Matcher<LogUploadEvent> MatchTrackerFinished(
    int64_t total,
    base::StringPiece access_parameters) {
  return Property(&LogUploadEvent::upload_tracker,
                  AllOf(Property(&UploadTracker::uploaded, Eq(total)),
                        Property(&UploadTracker::total, Eq(total)),
                        Property(&UploadTracker::session_token, IsEmpty()),
                        Property(&UploadTracker::access_parameters,
                                 StrEq(access_parameters))));
}

::testing::Matcher<LogUploadEvent> MatchError(Status status) {
  return Property(&LogUploadEvent::upload_tracker,
                  Property(&UploadTracker::status,
                           AllOf(Property(&StatusProto::code, status.code()),
                                 Property(&StatusProto::error_message,
                                          StrEq(status.error_message())))));
}

TEST_F(RecordHandlerUploadTest, SuccessfulInitiation) {
  EncryptedRecord init_encrypted_record = ComposeEncryptedRecord(
      "Init Upload Record", ComposeUploadSettings(1), absl::nullopt);
  ScopedReservation record_reservation(init_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information = init_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate(StrEq("/tmp/file"), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](base::StringPiece origin_path, base::StringPiece upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, "ABC"));
          }));
  EXPECT_CALL(*delegate, DoNextStep).Times(0);
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(), MatchTrackerInProgress(0L, 300L, "ABC")));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  handler.HandleRecords(/*need_encryption_key=*/false,
                        std::vector(1, std::move(init_encrypted_record)),
                        std::move(record_reservation), responder_event.cb(),
                        encryption_key_attached_event.repeating_cb());
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, SuccessfulNextStep) {
  EncryptedRecord next_step_encrypted_record =
      ComposeEncryptedRecord("Step Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 100L));
  ScopedReservation record_reservation(
      next_step_encrypted_record.ByteSizeLong(), memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep(Eq(300L), Eq(100L), StrEq("ABC"), _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, base::StringPiece session_token,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(), MatchTrackerInProgress(200L, 300L, "ABC")));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));
  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  handler.HandleRecords(/*need_encryption_key=*/false,
                        std::vector(1, std::move(next_step_encrypted_record)),
                        std::move(record_reservation), responder_event.cb(),
                        encryption_key_attached_event.repeating_cb());
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, SuccessfulFinalize) {
  EncryptedRecord fin_encrypted_record =
      ComposeEncryptedRecord("Finish Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 300L));
  ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep).Times(0);
  EXPECT_CALL(*delegate, DoFinalize(StrEq("ABC"), _))
      .WillOnce(
          Invoke([](base::StringPiece session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run("http://destination");
          }));

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerFinished(300L, "http://destination")));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));
  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  handler.HandleRecords(/*need_encryption_key=*/false,
                        std::vector(1, std::move(fin_encrypted_record)),
                        std::move(record_reservation), responder_event.cb(),
                        encryption_key_attached_event.repeating_cb());
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, AlreadyFinalized) {
  EncryptedRecord fin_encrypted_record =
      ComposeEncryptedRecord("Finish Upload Record", ComposeUploadSettings(),
                             ComposeDoneTracker(300L));
  ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep).Times(0);
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerFinished(300L, "http://destination")));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  handler.HandleRecords(/*need_encryption_key=*/false,
                        std::vector(1, std::move(fin_encrypted_record)),
                        std::move(record_reservation), responder_event.cb(),
                        encryption_key_attached_event.repeating_cb());
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, FailedProcessing) {
  EncryptedRecord next_step_encrypted_record =
      ComposeEncryptedRecord("Step Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 100L));
  ScopedReservation record_reservation(
      next_step_encrypted_record.ByteSizeLong(), memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep(Eq(300L), Eq(100L), StrEq("ABC"), _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, base::StringPiece session_token,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(Status(error::CANCELLED, "Failure by test"));
          });
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(),
                  MatchError(Status(error::CANCELLED, "Failure by test"))));
        std::move(callback).Run(Status::StatusOK());
      }));
  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  handler.HandleRecords(/*need_encryption_key=*/false,
                        std::vector(1, std::move(next_step_encrypted_record)),
                        std::move(record_reservation), responder_event.cb(),
                        encryption_key_attached_event.repeating_cb());
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, RepeatedInitiationAttempts) {
  static constexpr int64_t kNumTestRecords = 10;

  EncryptedRecord init_encrypted_record = ComposeEncryptedRecord(
      "Init Upload Record", ComposeUploadSettings(1), absl::nullopt);
  SuccessfulUploadResponse expected_response{
      .sequence_information = init_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate(StrEq("/tmp/file"), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](base::StringPiece origin_path, base::StringPiece upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, "ABC"));
          }));
  EXPECT_CALL(*delegate, DoNextStep).Times(0);
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(
          Invoke([](Priority priority, Record record,
                    StorageModuleInterface::EnqueueCallback callback) {
            EXPECT_TRUE(record.needs_local_unencrypted_copy());
            LogUploadEvent log_upload_event;
            EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                        record.data().size()));
            EXPECT_THAT(log_upload_event,
                        AllOf(MatchSettings(),
                              MatchTrackerInProgress(0L, 300L, "ABC")));
            EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
            std::move(callback).Run(Status::StatusOK());
          }));

  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(init_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler.HandleRecords(/*need_encryption_key=*/false,
                          std::vector(1, init_encrypted_record),
                          std::move(record_reservation), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
    init_encrypted_record.mutable_sequence_information()->set_sequencing_id(
        init_encrypted_record.sequence_information().sequencing_id() + 1);
    expected_response.sequence_information.set_sequencing_id(
        init_encrypted_record.sequence_information().sequencing_id());
  }
}

TEST_F(RecordHandlerUploadTest, RepeatedNextStepAttempts) {
  static constexpr int64_t kNumTestRecords = 10;

  EncryptedRecord next_step_encrypted_record =
      ComposeEncryptedRecord("Step Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 100L));
  SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep(Eq(300L), Eq(100L), StrEq("ABC"), _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, base::StringPiece session_token,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  EXPECT_CALL(*delegate, DoFinalize).Times(0);

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(
          Invoke([](Priority priority, Record record,
                    StorageModuleInterface::EnqueueCallback callback) {
            EXPECT_TRUE(record.needs_local_unencrypted_copy());
            LogUploadEvent log_upload_event;
            EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                        record.data().size()));
            EXPECT_THAT(log_upload_event,
                        AllOf(MatchSettings(),
                              MatchTrackerInProgress(200L, 300L, "ABC")));
            EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
            std::move(callback).Run(Status::StatusOK());
          }));

  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(
        next_step_encrypted_record.ByteSizeLong(), memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler.HandleRecords(/*need_encryption_key=*/false,
                          std::vector(1, next_step_encrypted_record),
                          std::move(record_reservation), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
    next_step_encrypted_record.mutable_sequence_information()
        ->set_sequencing_id(
            next_step_encrypted_record.sequence_information().sequencing_id() +
            1);
    expected_response.sequence_information.set_sequencing_id(
        next_step_encrypted_record.sequence_information().sequencing_id());
  }
}

TEST_F(RecordHandlerUploadTest, RepeatedFinalizeAttempts) {
  static constexpr int64_t kNumTestRecords = 10;

  EncryptedRecord fin_encrypted_record =
      ComposeEncryptedRecord("Finish Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 300L));
  SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(*test_env_.client(),
              UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetSuccess(true))));

  auto delegate = std::make_unique<MockFileUploadDelegate>();
  EXPECT_CALL(*delegate, DoInitiate).Times(0);
  EXPECT_CALL(*delegate, DoNextStep).Times(0);
  EXPECT_CALL(*delegate, DoFinalize(StrEq("ABC"), _))
      .WillOnce(
          Invoke([](base::StringPiece session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run("http://destination");
          }));

  auto storage = base::MakeRefCounted<test::TestStorageModule>();
  EXPECT_CALL(*storage, AddRecord(Eq(Priority::IMMEDIATE), _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(
          Invoke([](Priority priority, Record record,
                    StorageModuleInterface::EnqueueCallback callback) {
            EXPECT_FALSE(record.needs_local_unencrypted_copy());
            LogUploadEvent log_upload_event;
            EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                        record.data().size()));
            EXPECT_THAT(
                log_upload_event,
                AllOf(MatchSettings(),
                      MatchTrackerFinished(300L, "http://destination")));
            EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
            std::move(callback).Run(Status::StatusOK());
          }));

  RecordHandlerImpl handler(sequenced_task_runner_, std::move(delegate),
                            storage);
  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler.HandleRecords(/*need_encryption_key=*/false,
                          std::vector(1, fin_encrypted_record),
                          std::move(record_reservation), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
    fin_encrypted_record.mutable_sequence_information()->set_sequencing_id(
        fin_encrypted_record.sequence_information().sequencing_id() + 1);
    expected_response.sequence_information.set_sequencing_id(
        fin_encrypted_record.sequence_information().sequencing_id());
  }
}
}  // namespace
}  // namespace reporting
