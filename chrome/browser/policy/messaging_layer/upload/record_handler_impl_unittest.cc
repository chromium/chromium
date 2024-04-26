// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/expected.h"
#include "base/uuid.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job_test_util.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/upload/server_uploader.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector.h"
#include "chrome/browser/policy/messaging_layer/util/reporting_server_connector_test_util.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::ContainerEq;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::IsEmpty;
using ::testing::Not;
using testing::NotNull;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrEq;

namespace reporting {
namespace {

static constexpr size_t kNumTestRecords = 10;
static constexpr int64_t kGenerationId = 1234;

MATCHER_P(ResponseEquals,
          expected,
          "Compares StatusOr<response> to expected response") {
  if (!arg.has_value()) {
    return false;
  }
  if (arg.value().sequence_information.GetTypeName() !=
      expected.sequence_information.GetTypeName()) {
    return false;
  }
  if (arg.value().sequence_information.SerializeAsString() !=
      expected.sequence_information.SerializeAsString()) {
    return false;
  }
  return arg.value().force_confirm == expected.force_confirm;
}

class MockFileUploadDelegate : public FileUploadJob::Delegate {
 public:
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
class RecordHandlerImplTest : public ::testing::TestWithParam<
                                  ::testing::tuple</*need_encryption_key*/ bool,
                                                   /*force_confirm*/ bool>> {
 public:
  const std::string kGenerationGuid =
      base::Uuid::GenerateRandomV4().AsLowercaseString();

 protected:
  void SetUp() override {
    handler_ = std::make_unique<RecordHandlerImpl>(
        base::SequencedTaskRunner::GetCurrentDefault(),
        base::BindRepeating([]() -> FileUploadJob::Delegate::SmartPtr {
          return FileUploadJob::Delegate::SmartPtr(
              new MockFileUploadDelegate(),
              base::OnTaskRunnerDeleter(FileUploadJob::Manager::GetInstance()
                                            ->sequenced_task_runner()));
        }));
    test_reporting_ =
        ReportingClient::TestEnvironment::CreateWithStorageModule();
    test_env_ = std::make_unique<ReportingServerConnector::TestEnvironment>();

    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(4u * 1024LLu * 1024LLu);  // 4 MiB
  }

  void TearDown() override {
    handler_.reset();
    test_reporting_.reset();
    test_env_.reset();
    EXPECT_THAT(memory_resource_->GetUsed(), Eq(0uL));
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_environment_;

  // Set up this device as a managed device.
  policy::ScopedManagementServiceOverrideForTesting scoped_management_service_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  FileUploadJob::TestEnvironment manager_test_env_;
  std::unique_ptr<ReportingServerConnector::TestEnvironment> test_env_;

  std::unique_ptr<ReportingClient::TestEnvironment> test_reporting_;

  std::unique_ptr<RecordHandlerImpl> handler_;

  scoped_refptr<ResourceManager> memory_resource_;
};

std::pair<ScopedReservation, std::vector<EncryptedRecord>>
BuildTestRecordsVector(size_t number_of_test_records,
                       int64_t generation_id,
                       std::string generation_guid,
                       scoped_refptr<ResourceManager> memory_resource) {
  ScopedReservation total_reservation;
  std::vector<EncryptedRecord> test_records;
  test_records.reserve(number_of_test_records);
  for (size_t i = 0; i < number_of_test_records; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(
        base::StrCat({"Record Number ", base::NumberToString(i)}));
    auto* sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_generation_id(generation_id);
#if BUILDFLAG(IS_CHROMEOS)
    sequence_information->set_generation_guid(generation_guid);
#endif  // BUILDFLAG(IS_CHROMEOS)
    sequence_information->set_sequencing_id(i);
    sequence_information->set_priority(Priority::IMMEDIATE);
    ScopedReservation record_reservation(encrypted_record.ByteSizeLong(),
                                         memory_resource);
    test_records.push_back(std::move(encrypted_record));
    total_reservation.HandOver(record_reservation);
  }
  return std::make_pair(std::move(total_reservation), std::move(test_records));
}

std::list<int64_t> GetExpectedCachedSeqIds(
    const std::vector<EncryptedRecord>& records) {
  std::list<int64_t> seq_ids;
  for (const auto& record : records) {
    seq_ids.push_back(record.sequence_information().sequencing_id());
  }
  return seq_ids;
}

TEST_P(RecordHandlerImplTest, UploadRecords) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  SuccessfulUploadResponse expected_response{
      .sequence_information = test_records.second.back().sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

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
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result);
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());
  response->RemoveByDottedPath("lastSucceedUploadedRecord.priority");
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}

TEST_P(RecordHandlerImplTest, InvalidPriorityField) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;

  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body,
              RequestValidityMatcherBuilder<>::CreateDataUpload()
                  .RemoveMatcher("sequence-information-record-matcher")
                  .Build());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());
  response->SetByDottedPath("lastSucceedUploadedRecord.priority", "abc");
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}

TEST_P(RecordHandlerImplTest, ContainsGenerationGuid) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());

#if BUILDFLAG(IS_CHROMEOS)
  // Verify generation guid exists and equals kGenerationGuid.
  ASSERT_THAT(response->FindStringByDottedPath(
                  "lastSucceedUploadedRecord.generationGuid"),
              NotNull());
  EXPECT_THAT(*(response->FindStringByDottedPath(
                  "lastSucceedUploadedRecord.generationGuid")),
              StrEq(kGenerationGuid));
#endif  // BUILDFLAG(IS_CHROMEOS)

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_OK(result) << result.error();
}

TEST_P(RecordHandlerImplTest, ValidGenerationGuid) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());

  // Respond with a valid generation guid. Generation guids are random UUID
  // strings and can be verified by parsing the string into a UUID class using
  // UUID class functions.
  response->SetByDottedPath("lastSucceedUploadedRecord.generationGuid",
                            base::Uuid::GenerateRandomV4().AsLowercaseString());

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_OK(result) << result.error();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(RecordHandlerImplTest, InvalidGenerationGuid) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());

  // Generation guids must be parsable into `base::Uuid`.
  response->SetByDottedPath("lastSucceedUploadedRecord.generationGuid",
                            "invalid-generation-guid");

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(RecordHandlerImplTest, MissingGenerationGuidFromManagedDeviceIsOk) {
  // Set device as managed
  policy::ScopedManagementServiceOverrideForTesting scoped_management_service_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::CLOUD_DOMAIN);

  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());

  // Remove the generation guid. Managed devices are not required to have a
  // generation guid, since this is a new feature and legacy devices may not be
  // updated for some time.
  response->RemoveByDottedPath("lastSucceedUploadedRecord.generationGuid");

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_OK(result) << result.error();
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(RecordHandlerImplTest,
       MissingGenerationGuidFromUnmanagedDeviceReturnError) {
  // Set device as unmanaged
  policy::ScopedManagementServiceOverrideForTesting scoped_management_service_ =
      policy::ScopedManagementServiceOverrideForTesting(
          policy::ManagementServiceFactory::GetForPlatform(),
          policy::EnterpriseManagementAuthority::NONE);

  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());

  // Remove the generation guid. This should result in an error since we set
  // the device to an unmanaged state at the beginning of the test.
  response->RemoveByDottedPath("lastSucceedUploadedRecord.generationGuid");

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_FALSE(result.has_value());
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::INTERNAL)));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_P(RecordHandlerImplTest, MissingSequenceInformation) {
  // test records that has one record with missing sequence information.
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  SuccessfulUploadResponse expected_response{
      .sequence_information =
          test_records.second.back().sequence_information()};
  auto expected_cached_seq_ids = GetExpectedCachedSeqIds(test_records.second);

  // Corrupt sequence information of the last record and adjust expectations.
  expected_response.sequence_information.set_sequencing_id(
      test_records.second.back().sequence_information().sequencing_id() - 1L);
  expected_cached_seq_ids.pop_back();
  test_records.second.back().clear_sequence_information();

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body)).Build();
  ASSERT_TRUE(response.has_value());

  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  const auto result = responder_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));
}

TEST_P(RecordHandlerImplTest, ReportsUploadFailure) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<CompletionResponse> response_event;
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          response_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());

  test_env_->SimulateCustomResponseForRequest(
      0, base::unexpected(Status(error::INTERNAL, "Test injected error")));

  const auto result = response_event.result();
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::DATA_LOSS)));

  EXPECT_TRUE(encryption_key_attached_event.no_result());
}

// TODO(b/289534046): Uploading gap records immediately on response will be
// throttled by the uploading client (`CloudPolicyClient` or
// `EncryptedReportingClient`). We need to resolve the issue, update the test
// accordingly then re-enable it.
TEST_P(RecordHandlerImplTest, DISABLED_UploadsGapRecordOnServerFailure) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  const SuccessfulUploadResponse expected_response{
      .sequence_information =
          test_records.second.rbegin()->sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<CompletionResponse> response_event;
  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          response_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response =
      ResponseBuilder(std::move(request_body)).SetSuccess(false).Build();
  ASSERT_TRUE(response.has_value());
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  // Gap records upload.
  task_environment_.RunUntilIdle();
  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsGapUploadRequestValid());

  response = ResponseBuilder(std::move(request_body))
                 .SetForceConfirm(force_confirm_by_server)
                 .Build();
  ASSERT_TRUE(response.has_value());
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  // No more uploads expected.
  EXPECT_THAT(*test_env_->url_loader_factory()->pending_requests(), IsEmpty());

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
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<CompletionResponse> response_event;

  handler_->HandleRecords(
      need_encryption_key(), /*config_file_version=*/-1,
      std::move(test_records.second), std::move(test_records.first),
      enqueued_event.cb(), response_event.cb(),
      encryption_key_attached_event.repeating_cb(), base::DoNothing());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  EXPECT_THAT(test_env_->request_body(0), IsDataUploadRequestValid());

  test_env_->SimulateCustomResponseForRequest(0, base::Value::Dict());

  const auto result = response_event.result();
  EXPECT_THAT(result.error(),
              Property(&Status::error_code, Eq(error::INTERNAL)));

  EXPECT_TRUE(encryption_key_attached_event.no_result());
}

TEST_P(RecordHandlerImplTest, AssignsRequestIdForRecordUploads) {
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  SuccessfulUploadResponse expected_response{
      .sequence_information = test_records.second.back().sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<CompletionResponse> responder_event;
  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/-1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(), base::DoNothing(),
                          base::DoNothing());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1u));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  // We need to wait until the upload operation is marked complete (after it
  // triggers the response callback) so we can avoid leaking unmanaged
  // resources.
  const auto result = responder_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_P(RecordHandlerImplTest,
       ContainsConfigFileInResponseWithExperimentEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kShouldRequestConfigurationFile);

  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId,
                                             kGenerationGuid, memory_resource_);
  const auto force_confirm_by_server = force_confirm();
  const auto expected_cached_seq_ids =
      GetExpectedCachedSeqIds(test_records.second);

  SuccessfulUploadResponse expected_response{
      .sequence_information = test_records.second.back().sequence_information(),
      .force_confirm = force_confirm()};

  test::TestEvent<StatusOr<std::list<int64_t>>> enqueued_event;
  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<ConfigFile> config_file_attached_event;
  test::TestEvent<CompletionResponse> responder_event;

  handler_->HandleRecords(need_encryption_key(), /*config_file_version=*/1,
                          std::move(test_records.second),
                          std::move(test_records.first), enqueued_event.cb(),
                          responder_event.cb(),
                          encryption_key_attached_event.repeating_cb(),
                          config_file_attached_event.repeating_cb());
  const auto& enqueued_result = enqueued_event.result();
  ASSERT_OK(enqueued_result) << enqueued_result.error();
  EXPECT_THAT(enqueued_result.value(), ContainerEq(expected_cached_seq_ids));

  task_environment_.RunUntilIdle();

  ASSERT_THAT(*test_env_->url_loader_factory()->pending_requests(), SizeIs(1));
  auto request_body = test_env_->request_body(0);
  EXPECT_THAT(request_body, IsDataUploadRequestValid());
  auto response = ResponseBuilder(std::move(request_body))
                      .SetForceConfirm(force_confirm_by_server)
                      .Build();
  ASSERT_TRUE(response.has_value());
  test_env_->SimulateCustomResponseForRequest(0, std::move(response.value()));

  if (need_encryption_key()) {
    EXPECT_THAT(
        encryption_key_attached_event.result(),
        AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                       Not(IsEmpty())),
              Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
              Property(&SignedEncryptionInfo::signature, Not(IsEmpty()))));
  }

  EXPECT_THAT(
      config_file_attached_event.result(),
      AllOf(Property(&ConfigFile::config_file_signature, Not(IsEmpty())),
            Property(&ConfigFile::version, Gt(0)),
            Property(&ConfigFile::blocked_event_configs, Not(IsEmpty()))));
  const auto result = responder_event.result();
  EXPECT_THAT(result, ResponseEquals(expected_response));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    RecordHandlerImplTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
