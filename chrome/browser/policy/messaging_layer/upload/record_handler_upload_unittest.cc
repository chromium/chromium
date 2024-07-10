// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"
#include "chrome/browser/policy/messaging_layer/upload/server_uploader.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/proto/synced/upload_tracker.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "base/uuid.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

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
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::WithArgs;

using ::ash::reporting::LogUploadEvent;

namespace reporting {
namespace {

constexpr char kUploadFileName[] = "/tmp/file";
constexpr char kSessionToken[] = "ABC";
constexpr char kUploadParameters[] = "http://upload";
constexpr char kAccessParameters[] = "http://destination";

MATCHER_P(ResponseEquals,
          expected,
          "Compares StatusOr<response> to expected response") {
  if (!arg.has_value()) {
    *result_listener << "Failure status=" << arg.error();
    return false;
  }
  const auto& arg_value = arg.value();
  if (arg_value.sequence_information.GetTypeName() !=
      expected.sequence_information.GetTypeName()) {
    *result_listener << "Sequence info type mismatch: actual="
                     << arg_value.sequence_information.GetTypeName()
                     << ", expected="
                     << expected.sequence_information.GetTypeName();
    return false;
  }
  if (arg_value.sequence_information.SerializeAsString() !=
      expected.sequence_information.SerializeAsString()) {
    *result_listener << "Sequence info mismatch: actual="
                     << Priority_Name(arg_value.sequence_information.priority())
                     << "/" << arg_value.sequence_information.generation_id()
                     << "/" << arg_value.sequence_information.sequencing_id()
                     << "/" << arg_value.sequence_information.generation_guid()
                     << ", expected="
                     << Priority_Name(expected.sequence_information.priority())
                     << "/" << expected.sequence_information.generation_id()
                     << "/" << expected.sequence_information.sequencing_id()
                     << "/" << expected.sequence_information.generation_guid();
    return false;
  }
  if (arg_value.force_confirm != expected.force_confirm) {
    *result_listener << "Force commit mismatch: actual="
                     << arg_value.force_confirm
                     << ", expected=" << expected.force_confirm;
    return false;
  }
  return true;
}

class MockFileUploadDelegate : public FileUploadJob::Delegate {
 public:
  // Forwarder to `MockFileUploadDelegate` that allows to repeatedly construct
  // its instances and then forward the mocked calls to the single instance of
  // `MockFileUploadDelegate`.
  class Forwarder : public FileUploadJob::Delegate {
   public:
    explicit Forwarder(MockFileUploadDelegate* actual_mock)
        : actual_mock_(actual_mock) {}

    void DoInitiate(
        std::string_view origin_path,
        std::string_view upload_parameters,
        base::OnceCallback<void(
            StatusOr<std::pair<int64_t /*total*/,
                               std::string /*session_token*/>>)> cb) override {
      actual_mock_->DoInitiate(origin_path, upload_parameters, std::move(cb));
    }

    void DoNextStep(
        int64_t total,
        int64_t uploaded,
        std::string_view session_token,
        ScopedReservation scoped_reservation,
        base::OnceCallback<void(
            StatusOr<std::pair<int64_t /*uploaded*/,
                               std::string /*session_token*/>>)> cb) override {
      actual_mock_->DoNextStep(total, uploaded, session_token,
                               std::move(scoped_reservation), std::move(cb));
    }

    void DoFinalize(
        std::string_view session_token,
        base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
            cb) override {
      actual_mock_->DoFinalize(session_token, std::move(cb));
    }

    void DoDeleteFile(std::string_view origin_path) override {
      actual_mock_->DoDeleteFile(origin_path);
    }

   private:
    raw_ptr<MockFileUploadDelegate> actual_mock_;
  };

  MOCK_METHOD(void,
              DoInitiate,
              (std::string_view origin_path,
               std::string_view upload_parameters,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*total*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(void,
              DoNextStep,
              (int64_t total,
               int64_t uploaded,
               std::string_view session_token,
               ScopedReservation scoped_reservation,
               base::OnceCallback<void(
                   StatusOr<std::pair<int64_t /*uploaded*/,
                                      std::string /*session_token*/>>)> cb),
              (override));

  MOCK_METHOD(
      void,
      DoFinalize,
      (std::string_view session_token,
       base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
           cb),
      (override));

  MOCK_METHOD(void,
              DoDeleteFile,
              (std::string_view /*origin_path*/),
              (override));
};

// Tests for generic events handling.
class RecordHandlerUploadTest : public ::testing::Test {
 protected:
  void SetUp() override {
    test_storage_ = base::MakeRefCounted<test::TestStorageModule>();
    test_reporting_ = ReportingClient::TestEnvironment::CreateWithStorageModule(
        test_storage_);
    test_env_ = std::make_unique<ReportingServerConnector::TestEnvironment>();

    handler_ = std::make_unique<RecordHandlerImpl>(
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindRepeating(
            [](RecordHandlerUploadTest* self) {
              return FileUploadJob::Delegate::SmartPtr(
                  new MockFileUploadDelegate::Forwarder(&self->mock_delegate_),
                  base::OnTaskRunnerDeleter(
                      FileUploadJob::Manager::GetInstance()
                          ->sequenced_task_runner()));
            },
            base::Unretained(this)));

    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB

    // Create a queue and post event, in order to let ReportClient set storage.
    auto config_result =
        ReportQueueConfiguration::Create(
            {.event_type = EventType::kDevice, .destination = LOG_UPLOAD})
            .Build();
    EXPECT_TRUE(config_result.has_value()) << config_result.error();
    test::TestEvent<StatusOr<std::unique_ptr<ReportQueue>>> create_queue_event;
    ReportQueueProvider::CreateQueue(std::move(config_result.value()),
                                     create_queue_event.cb());
    auto report_queue_result = create_queue_event.result();
    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(report_queue_result.has_value()) << report_queue_result.error();

    // Enqueue event.
    test::TestEvent<Status> enqueue_record_event;
    std::move(report_queue_result.value())
        ->Enqueue("Record", FAST_BATCH, enqueue_record_event.cb());
    const auto enqueue_record_result = enqueue_record_event.result();
    EXPECT_OK(enqueue_record_result) << enqueue_record_result;
  }

  void TearDown() override {
    handler_.reset();
    test_env_.reset();
    test_storage_.reset();
    test_reporting_.reset();

    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  void VerifyUploadRequestAndRespond() {
    task_environment_.RunUntilIdle();

    ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(),
                SizeIs(1));
    base::Value::Dict request_body = test_env_->request_body(0);
    EXPECT_THAT(request_body, IsDataUploadRequestValid());

    test_env_->SimulateResponseForRequest(0);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Set up this device as a managed device.
  policy::ScopedManagementServiceOverrideForTesting scoped_management_service_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  FileUploadJob::TestEnvironment manager_test_env_;
  std::unique_ptr<ReportingServerConnector::TestEnvironment> test_env_;

  scoped_refptr<test::TestStorageModule> test_storage_;
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;

  MockFileUploadDelegate mock_delegate_;

  std::unique_ptr<RecordHandlerImpl> handler_;

  scoped_refptr<ResourceManager> memory_resource_;
};

EncryptedRecord ComposeEncryptedRecord(
    std::string_view data,
    UploadSettings upload_settings,
    std::optional<UploadTracker> upload_tracker) {
  static constexpr int64_t kGenerationId = 1234;
  EncryptedRecord encrypted_record;
  encrypted_record.set_encrypted_wrapped_record(data.data(), data.size());
  auto* sequence_information = encrypted_record.mutable_sequence_information();
  sequence_information->set_generation_id(kGenerationId);
#if BUILDFLAG(IS_CHROMEOS)
  static const std::string kGenerationGuid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();
  sequence_information->set_generation_guid(kGenerationGuid);
#endif  // BUILDFLAG(IS_CHROMEOS)
  sequence_information->set_sequencing_id(0);
  sequence_information->set_priority(Priority::SECURITY);
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

UploadSettings ComposeUploadSettings(int64_t retry_count = 1) {
  UploadSettings settings;
  settings.set_origin_path(kUploadFileName);
  settings.set_retry_count(retry_count);
  settings.set_upload_parameters(kUploadParameters);
  return settings;
}

UploadTracker ComposeUploadTracker(int64_t total, int64_t uploaded) {
  UploadTracker tracker;
  tracker.set_total(total);
  tracker.set_uploaded(uploaded);
  tracker.set_session_token(kSessionToken);
  return tracker;
}

UploadTracker ComposeDoneTracker(int64_t total) {
  UploadTracker tracker;
  tracker.set_total(total);
  tracker.set_uploaded(tracker.total());
  tracker.set_access_parameters(kAccessParameters);
  return tracker;
}

::testing::Matcher<LogUploadEvent> MatchSettings(int64_t retry_count = 1) {
  return Property(
      &LogUploadEvent::upload_settings,
      AllOf(Property(&UploadSettings::retry_count, Eq(retry_count)),
            Property(&UploadSettings::origin_path, StrEq(kUploadFileName)),
            Property(&UploadSettings::upload_parameters,
                     StrEq(kUploadParameters))));
}

::testing::Matcher<LogUploadEvent> MatchTrackerInProgress(
    int64_t uploaded,
    int64_t total,
    std::string_view session_token) {
  return Property(
      &LogUploadEvent::upload_tracker,
      AllOf(Property(&UploadTracker::uploaded, Eq(uploaded)),
            Property(&UploadTracker::total, Eq(total)),
            Property(&UploadTracker::session_token, StrEq(session_token)),
            Property(&UploadTracker::access_parameters, IsEmpty())));
}

::testing::Matcher<LogUploadEvent> MatchTrackerFinished(
    int64_t total,
    std::string_view access_parameters) {
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
      "Init Upload Record", ComposeUploadSettings(), std::nullopt);
  ScopedReservation record_reservation(init_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = init_encrypted_record.sequence_information(),
      .force_confirm = false};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  EXPECT_CALL(mock_delegate_,
              DoInitiate(StrEq(kUploadFileName), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerInProgress(0L, 300L, kSessionToken)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  handler_->HandleRecords(
      /*need_encryption_key=*/false,
      /*config_file_version=*/-1,
      std::vector(1, std::move(init_encrypted_record)),
      std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
      responder_event.cb(), encryption_key_attached_event.repeating_cb(),
      config_file_attached_event.repeating_cb());
  VerifyUploadRequestAndRespond();
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, SuccessfulNextStep) {
  EncryptedRecord next_step_encrypted_record =
      ComposeEncryptedRecord("Step Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 100L));
  ScopedReservation record_reservation(
      next_step_encrypted_record.ByteSizeLong(), memory_resource_);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_,
              DoNextStep(Eq(300L), Eq(100L), StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerInProgress(200L, 300L, kSessionToken)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));
  handler_->HandleRecords(
      /*need_encryption_key=*/false,
      /*config_file_version=*/-1,
      std::vector(1, std::move(next_step_encrypted_record)),
      std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
      responder_event.cb(), encryption_key_attached_event.repeating_cb(),
      config_file_attached_event.repeating_cb());
  VerifyUploadRequestAndRespond();
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, SuccessfulFinalize) {
  EncryptedRecord fin_encrypted_record =
      ComposeEncryptedRecord("Finish Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 300L));
  ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(kAccessParameters);
          }));
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName))).Times(1);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerFinished(300L, kAccessParameters)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));
  handler_->HandleRecords(
      /*need_encryption_key=*/false,
      /*config_file_version=*/-1,
      std::vector(1, std::move(fin_encrypted_record)),
      std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
      responder_event.cb(), encryption_key_attached_event.repeating_cb(),
      config_file_attached_event.repeating_cb());
  VerifyUploadRequestAndRespond();
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, AlreadyFinalized) {
  EncryptedRecord fin_encrypted_record =
      ComposeEncryptedRecord("Finish Upload Record", ComposeUploadSettings(),
                             ComposeDoneTracker(300L));
  ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                       memory_resource_);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  EXPECT_CALL(*test_storage_, AddRecord).Times(0);

  handler_->HandleRecords(
      /*need_encryption_key=*/false,
      /*config_file_version=*/-1,
      std::vector(1, std::move(fin_encrypted_record)),
      std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
      responder_event.cb(), encryption_key_attached_event.repeating_cb(),
      config_file_attached_event.repeating_cb());
  VerifyUploadRequestAndRespond();
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, FailedProcessing) {
  EncryptedRecord next_step_encrypted_record =
      ComposeEncryptedRecord("Step Upload Record", ComposeUploadSettings(),
                             ComposeUploadTracker(300L, 100L));
  ScopedReservation record_reservation(
      next_step_encrypted_record.ByteSizeLong(), memory_resource_);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_,
              DoNextStep(Eq(300L), Eq(100L), StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Failure by test")));
          });
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName))).Times(1);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
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
  handler_->HandleRecords(
      /*need_encryption_key=*/false,
      /*config_file_version=*/-1,
      std::vector(1, std::move(next_step_encrypted_record)),
      std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
      responder_event.cb(), encryption_key_attached_event.repeating_cb(),
      config_file_attached_event.repeating_cb());
  VerifyUploadRequestAndRespond();
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_F(RecordHandlerUploadTest, RepeatedInitiationAttempts) {
  static constexpr int64_t kNumTestRecords = 10;

  EncryptedRecord init_encrypted_record = ComposeEncryptedRecord(
      "Init Upload Record", ComposeUploadSettings(), std::nullopt);
  SuccessfulUploadResponse expected_response{
      .sequence_information = init_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(mock_delegate_,
              DoInitiate(StrEq(kUploadFileName), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(std::make_pair(300L, kSessionToken));
          }));
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerInProgress(0L, 300L, kSessionToken)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(init_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false, /*config_file_version=*/-1,
        std::vector(1, init_encrypted_record), std::move(record_reservation),
        /*enqueued_cb=*/base::DoNothing(), responder_event.cb(),
        encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
    init_encrypted_record.mutable_sequence_information()->set_sequencing_id(
        init_encrypted_record.sequence_information().sequencing_id() + 1);
    expected_response.sequence_information.set_sequencing_id(
        init_encrypted_record.sequence_information().sequencing_id());
  }
}

TEST_F(RecordHandlerUploadTest, InitiationFailureTriggersRetry) {
  EncryptedRecord init_encrypted_record = ComposeEncryptedRecord(
      "Init Upload Record", ComposeUploadSettings(/*retry_count=*/2),
      std::nullopt);
  const SuccessfulUploadResponse expected_response{
      .sequence_information = init_encrypted_record.sequence_information(),
      .force_confirm = false};

  // Simulate delegate failure initiating the job.
  EXPECT_CALL(mock_delegate_,
              DoInitiate(StrEq(kUploadFileName), Not(IsEmpty()), _))
      .WillOnce(Invoke(
          [](std::string_view origin_path, std::string_view upload_parameters,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*total*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Failure by test")));
          }));
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  // Record retry event and then original with status.
  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect retry event (starting a new job) - no tracker.
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event, MatchSettings(/*retry_count=*/1));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect initiation failed event.
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(/*retry_count=*/2),
                  MatchError(Status(error::CANCELLED, "Failure by test"))));
        std::move(callback).Run(Status::StatusOK());
      }));

  // Original file to not be deleted!
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

  {
    ScopedReservation record_reservation(init_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false,
        /*config_file_version=*/-1,
        std::vector(1, std::move(init_encrypted_record)),
        std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
        responder_event.cb(), encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
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

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  // Simulate delegate failure making the next step of the job,
  EXPECT_CALL(mock_delegate_,
              DoNextStep(Eq(300L), Eq(100L), StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                std::make_pair(uploaded + 100L, std::string(session_token)));
          });
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerInProgress(200L, 300L, kSessionToken)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(
        next_step_encrypted_record.ByteSizeLong(), memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false,
        /*config_file_version=*/-1, std::vector(1, next_step_encrypted_record),
        std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
        responder_event.cb(), encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
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

TEST_F(RecordHandlerUploadTest, NextStepFailureTriggersRetry) {
  EncryptedRecord next_step_encrypted_record = ComposeEncryptedRecord(
      "Step Upload Record", ComposeUploadSettings(/*retry_count=*/2),
      ComposeUploadTracker(300L, 100L));
  const SuccessfulUploadResponse expected_response{
      .sequence_information = next_step_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_,
              DoNextStep(Eq(300L), Eq(100L), StrEq(kSessionToken), _, _))
      .WillOnce(
          [](int64_t total, int64_t uploaded, std::string_view session_token,
             ScopedReservation scoped_reservation,
             base::OnceCallback<void(
                 StatusOr<std::pair<int64_t /*uploaded*/,
                                    std::string /*session_token*/>>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Failure by test")));
          });
  EXPECT_CALL(mock_delegate_, DoFinalize).Times(0);

  // Record retry event and then original with status.
  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect retry event (starting a new job) - no tracker.
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event, MatchSettings(/*retry_count=*/1));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect next step failed event.
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(/*retry_count=*/2),
                  MatchError(Status(error::CANCELLED, "Failure by test"))));
        std::move(callback).Run(Status::StatusOK());
      }));

  // Original file to not be deleted!
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

  {
    ScopedReservation record_reservation(
        next_step_encrypted_record.ByteSizeLong(), memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false, /*config_file_version=*/-1,
        std::vector(1, std::move(next_step_encrypted_record)),
        std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
        responder_event.cb(), encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
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

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(kAccessParameters);
          }));

  // Record added only once!
  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event,
                    AllOf(MatchSettings(),
                          MatchTrackerFinished(300L, kAccessParameters)));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }));

  // Original file to be deleted only once!
  EXPECT_CALL(mock_delegate_, DoDeleteFile(StrEq(kUploadFileName))).Times(1);

  for (size_t i = 0; i < kNumTestRecords; ++i) {
    ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false, /*config_file_version=*/-1,
        std::vector(1, fin_encrypted_record), std::move(record_reservation),
        /*enqueued_cb=*/base::DoNothing(), responder_event.cb(),
        encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
    fin_encrypted_record.mutable_sequence_information()->set_sequencing_id(
        fin_encrypted_record.sequence_information().sequencing_id() + 1);
    expected_response.sequence_information.set_sequencing_id(
        fin_encrypted_record.sequence_information().sequencing_id());
  }
}

TEST_F(RecordHandlerUploadTest, FinalizeFailureTriggersRetry) {
  EncryptedRecord fin_encrypted_record = ComposeEncryptedRecord(
      "Finish Upload Record", ComposeUploadSettings(/*retry_count=*/2),
      ComposeUploadTracker(300L, 300L));
  const SuccessfulUploadResponse expected_response{
      .sequence_information = fin_encrypted_record.sequence_information(),
      .force_confirm = false};

  EXPECT_CALL(mock_delegate_, DoInitiate).Times(0);
  EXPECT_CALL(mock_delegate_, DoNextStep).Times(0);
  // Simulate delegate failure finalizing the job.
  EXPECT_CALL(mock_delegate_, DoFinalize(StrEq(kSessionToken), _))
      .WillOnce(
          Invoke([](std::string_view session_token,
                    base::OnceCallback<void(
                        StatusOr<std::string /*access_parameters*/>)> cb) {
            std::move(cb).Run(
                base::unexpected(Status(error::CANCELLED, "Failure by test")));
          }));

  // Record retry event and then original with status.
  EXPECT_CALL(*test_storage_, AddRecord(Eq(Priority::SECURITY), _, _))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect retry event (starting a new job) - no tracker.
        EXPECT_TRUE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(log_upload_event, MatchSettings(/*retry_count=*/1));
        EXPECT_FALSE(log_upload_event.upload_tracker().has_status());
        std::move(callback).Run(Status::StatusOK());
      }))
      .WillOnce(Invoke([](Priority priority, Record record,
                          StorageModuleInterface::EnqueueCallback callback) {
        // Expect finalize failed event.
        EXPECT_FALSE(record.needs_local_unencrypted_copy());
        LogUploadEvent log_upload_event;
        EXPECT_TRUE(log_upload_event.ParseFromArray(record.data().data(),
                                                    record.data().size()));
        EXPECT_THAT(
            log_upload_event,
            AllOf(MatchSettings(/*retry_count=*/2),
                  MatchError(Status(error::CANCELLED, "Failure by test"))));
        std::move(callback).Run(Status::StatusOK());
      }));

  // Original file to not be deleted!
  EXPECT_CALL(mock_delegate_, DoDeleteFile).Times(0);

  {
    ScopedReservation record_reservation(fin_encrypted_record.ByteSizeLong(),
                                         memory_resource_);
    test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
    test::TestEvent<ConfigFile> config_file_attached_event;
    test::TestEvent<CompletionResponse> responder_event;
    handler_->HandleRecords(
        /*need_encryption_key=*/false,
        /*config_file_version=*/-1,
        std::vector(1, std::move(fin_encrypted_record)),
        std::move(record_reservation), /*enqueued_cb=*/base::DoNothing(),
        responder_event.cb(), encryption_key_attached_event.repeating_cb(),
        config_file_attached_event.repeating_cb());
    VerifyUploadRequestAndRespond();
    auto response = responder_event.result();
    EXPECT_THAT(response, ResponseEquals(expected_response));
  }
}
}  // namespace
}  // namespace reporting
