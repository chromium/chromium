// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/task/task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_uploader.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#endif

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::WithArgs;

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
               ScopedReservation scoped_reservation,
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

  MOCK_METHOD(void,
              DoDeleteFile,
              (base::StringPiece /*origin_path*/),
              (override));
};

// Tests for generic events handling.
class RecordHandlerImplTest : public ::testing::TestWithParam<
                                  ::testing::tuple</*need_encryption_key*/ bool,
                                                   /*force_confirm*/ bool>> {
 protected:
  void SetUp() override {
    handler_ = std::make_unique<RecordHandlerImpl>(
        base::SequencedTaskRunner::GetCurrentDefault(),
        std::make_unique<MockFileUploadDelegate>());
    test_storage_ = base::MakeRefCounted<test::TestStorageModule>();
    test_reporting_ = ReportingClient::TestEnvironment::CreateWithStorageModule(
        test_storage_);

    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
  }

  void TearDown() override {
    handler_.reset();
    test_reporting_.reset();
    test_storage_.reset();
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_environment_;

  FileUploadJob::TestEnvironment manager_test_env_;
  ReportingServerConnector::TestEnvironment test_env_;

  scoped_refptr<test::TestStorageModule> test_storage_;
  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;

  std::unique_ptr<RecordHandlerImpl> handler_;

  scoped_refptr<ResourceManager> memory_resource_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Control device management status for Ash.
  std::unique_ptr<ash::ScopedStubInstallAttributes> install_attributes_ =
      std::make_unique<ash::ScopedStubInstallAttributes>(
          ash::StubInstallAttributes::CreateCloudManaged("fake-domain-name",
                                                         "fake-device-id"));
#endif
};

std::pair<ScopedReservation, std::vector<EncryptedRecord>>
BuildTestRecordsVector(int64_t number_of_test_records,
                       int64_t generation_id,
                       scoped_refptr<ResourceManager> memory_resource) {
  ScopedReservation total_reservation;
  std::vector<EncryptedRecord> test_records;
  test_records.reserve(number_of_test_records);
  for (int64_t i = 0; i < number_of_test_records; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(
        base::StrCat({"Record Number ", base::NumberToString(i)}));
    auto* sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_generation_id(generation_id);
    sequence_information->set_sequencing_id(i);
    sequence_information->set_priority(Priority::IMMEDIATE);
    ScopedReservation record_reservation(encrypted_record.ByteSizeLong(),
                                         memory_resource);
    test_records.push_back(std::move(encrypted_record));
    total_reservation.HandOver(record_reservation);
  }
  return std::make_pair(std::move(total_reservation), std::move(test_records));
}

TEST_P(RecordHandlerImplTest, UploadRecords) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  const auto force_confirm_by_server = force_confirm();

  SuccessfulUploadResponse expected_response{
      .sequence_information = test_records.second.back().sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  absl::optional<base::Value::Dict> response =
      ResponseBuilder(std::move(request_body))
          .SetForceConfirm(force_confirm_by_server)
          .Build();
  ASSERT_TRUE(response.has_value());
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  if (need_encryption_key()) {
    EXPECT_THAT(
        encryption_key_attached_event.result(),
        AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                       Not(IsEmpty())),
              Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
              Property(&SignedEncryptionInfo::signature, Not(IsEmpty()))));
  }
  const auto result = responder_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));
}

TEST_P(RecordHandlerImplTest, MissingPriorityField) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  const auto force_confirm_by_server = force_confirm();

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  absl::optional<base::Value::Dict> response =
      ResponseBuilder(std::move(request_body))
          .SetForceConfirm(force_confirm_by_server)
          .Build();
  ASSERT_TRUE(response.has_value());
  response->RemoveByDottedPath("lastSucceedUploadedRecord.priority");
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}

TEST_P(RecordHandlerImplTest, InvalidPriorityField) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  const auto force_confirm_by_server = force_confirm();

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body,
              RequestValidityMatcherBuilder<>::CreateDataUpload()
                  .RemoveMatcher("sequence-information-record-matcher")
                  .Build());
  absl::optional<base::Value::Dict> response =
      ResponseBuilder(std::move(request_body))
          .SetForceConfirm(force_confirm_by_server)
          .Build();
  ASSERT_TRUE(response.has_value());
  response->SetByDottedPath("lastSucceedUploadedRecord.priority", "abc");
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}

TEST_P(RecordHandlerImplTest, MissingSequenceInformation) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  // test records that has one record with missing sequence information.
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  test_records.second.back().clear_sequence_information();

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), responder_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  // The result should show an error and UploadEncryptedReport should not have
  // been even called, because UploadEncryptedReportingRequestBuilder::Build()
  // should fail in this situation.
  EXPECT_THAT(*test_env_.url_loader_factory()->pending_requests(), IsEmpty());

  const auto result = responder_event.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::FAILED_PRECONDITION)));
}

TEST_P(RecordHandlerImplTest, ReportsUploadFailure) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);

  test::TestEvent<CompletionResponse> response_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), response_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());

  test_env_.SimulateCustomResponseForRequest(0, absl::nullopt);

  const auto result = response_event.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::DATA_LOSS)));

  EXPECT_TRUE(encryption_key_attached_event.no_result());
}

// TODO(b/289534046): Uploading gap records immediately on response will be
// throttled by the uploading client (`CloudPolicyClient` or
// `EncryptedReportingClient`). We need to resolve the issue, update the test
// accordingly then re-enable it.
TEST_P(RecordHandlerImplTest, DISABLED_UploadsGapRecordOnServerFailure) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  const auto force_confirm_by_server = force_confirm();

  const SuccessfulUploadResponse expected_response{
      .sequence_information =
          test_records.second.rbegin()->sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<CompletionResponse> response_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), response_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  absl::optional<base::Value::Dict> response =
      ResponseBuilder(std::move(request_body)).SetSuccess(false).Build();
  ASSERT_TRUE(response.has_value());
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  // Gap records upload.
  task_environment_.RunUntilIdle();
  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsGapUploadRequestValid());

  response = ResponseBuilder(std::move(request_body))
                 .SetForceConfirm(force_confirm_by_server)
                 .Build();
  ASSERT_TRUE(response.has_value());
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  // No more uploads expected.
  EXPECT_THAT(*test_env_.url_loader_factory()->pending_requests(), IsEmpty());

  const auto result = response_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));

  if (need_encryption_key()) {
    EXPECT_THAT(
        encryption_key_attached_event.result(),
        AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                       Not(IsEmpty())),
              Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
              Property(&SignedEncryptionInfo::signature, Not(IsEmpty()))));
  } else {
    EXPECT_TRUE(encryption_key_attached_event.no_result());
  }
}

// There may be cases where the server and the client do not align in the
// expected response, clients shouldn't crash in these instances, but simply
// report an internal error.
TEST_P(RecordHandlerImplTest, HandleUnknownResponseFromServer) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> response_event;

  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), response_event.cb(),
                          encryption_key_attached_event.repeating_cb());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  EXPECT_THAT(test_env_.request_body(0), IsDataUploadRequestValid());

  test_env_.SimulateCustomResponseForRequest(0, base::Value::Dict());

  const auto result = response_event.result();
  EXPECT_THAT(result.status(),
              Property(&Status::error_code, Eq(error::INTERNAL)));

  EXPECT_TRUE(encryption_key_attached_event.no_result());
}

TEST_P(RecordHandlerImplTest, AssignsRequestIdForRecordUploads) {
  static constexpr int64_t kNumTestRecords = 1;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records =
      BuildTestRecordsVector(kNumTestRecords, kGenerationId, memory_resource_);
  const auto force_confirm_by_server = force_confirm();

  SuccessfulUploadResponse expected_response{
      .sequence_information = test_records.second.back().sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<CompletionResponse> responder_event;
  handler_->HandleRecords(need_encryption_key(), std::move(test_records.second),
                          std::move(test_records.first), responder_event.cb(),
                          base::DoNothing());
  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_.url_loader_factory()->pending_requests(), SizeIs(1));
  base::Value::Dict request_body = test_env_.request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  absl::optional<base::Value::Dict> response =
      ResponseBuilder(std::move(request_body))
          .SetForceConfirm(force_confirm_by_server)
          .Build();
  ASSERT_TRUE(response.has_value());
  test_env_.SimulateCustomResponseForRequest(0, std::move(response.value()));

  // We need to wait until the upload operation is marked complete (after it
  // triggers the response callback) so we can avoid leaking unmanaged
  // resources.
  const auto result = responder_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    RecordHandlerImplTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
