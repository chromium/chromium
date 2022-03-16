// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <tuple>

#include "base/base64.h"
#include "base/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/test.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::MockFunction;
using ::testing::Not;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
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

using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;

// TODO(https://crbug.com/1297261): Change the various out params in the helper
// functions below to return by value instead.

// Helper function for retrieving and processing the SequenceInformation from
// a request.
void RetrieveFinalSequenceInformation(const base::Value::Dict& request,
                                      base::Value::Dict& sequence_info) {
  // Retrieve and process sequence information
  const base::Value::List* const encrypted_record_list =
      request.FindList("encryptedRecord");
  ASSERT_TRUE(encrypted_record_list != nullptr);
  ASSERT_FALSE(encrypted_record_list->empty());
  const auto* const seq_info =
      encrypted_record_list->back().GetDict().FindDict("sequenceInformation");
  ASSERT_TRUE(seq_info != nullptr);
  ASSERT_TRUE(!seq_info->FindString("sequencingId")->empty());
  ASSERT_TRUE(!seq_info->FindString("generationId")->empty());
  ASSERT_TRUE(seq_info->FindInt("priority"));
  sequence_info.Merge(*seq_info);

  // Set half of sequence information to return a string instead of an int for
  // priority.
  int64_t sequencing_id;
  ASSERT_TRUE(base::StringToInt64(*sequence_info.FindString("sequencingId"),
                                  &sequencing_id));
  if (sequencing_id % 2) {
    const auto int_result = sequence_info.FindInt("priority");
    ASSERT_TRUE(int_result.has_value());
    sequence_info.Set("priority", Priority_Name(int_result.value()));
  }
}

absl::optional<base::Value> BuildEncryptionSettingsFromRequest(
    const base::Value::Dict& request) {
  // If attach_encryption_settings it true, process that.
  const auto attach_encryption_settings =
      request.FindBool("attachEncryptionSettings");
  if (!attach_encryption_settings.has_value() ||
      !attach_encryption_settings.value()) {
    return absl::nullopt;
  }

  base::Value encryption_settings{base::Value::Type::DICTIONARY};
  std::string public_key;
  base::Base64Encode("PUBLIC KEY", &public_key);
  encryption_settings.SetStringKey("publicKey", public_key);
  encryption_settings.SetIntKey("publicKeyId", 12345);
  std::string public_key_signature;
  base::Base64Encode("PUBLIC KEY SIG", &public_key_signature);
  encryption_settings.SetStringKey("publicKeySignature", public_key_signature);
  return encryption_settings;
}

// Immitates the server response for successful record upload. Since additional
// steps and tests require the response from the server to be accurate, ASSERTS
// that the |request| must be valid, and on a valid request updates |response|.
void SucceedResponseFromRequestHelper(const base::Value::Dict& request,
                                      bool force_confirm_by_server,
                                      base::Value::Dict& response,
                                      base::Value::Dict& sequence_info) {
  RetrieveFinalSequenceInformation(request, sequence_info);

  // If force_confirm is true, process that.
  if (force_confirm_by_server) {
    response.Set("forceConfirm", true);
  }

  // If attach_encryption_settings is true, process that.
  auto encryption_settings_result = BuildEncryptionSettingsFromRequest(request);
  if (encryption_settings_result.has_value()) {
    response.Set("encryptionSettings",
                 std::move(encryption_settings_result.value()));
  }
}

void SucceedResponseFromRequest(const base::Value::Dict& request,
                                bool force_confirm_by_server,
                                base::Value::Dict& response) {
  base::Value::Dict sequence_info;
  SucceedResponseFromRequestHelper(request, force_confirm_by_server, response,
                                   sequence_info);
  response.Set("lastSucceedUploadedRecord", std::move(sequence_info));
}

void SucceedResponseFromRequestMissingPriority(const base::Value::Dict& request,
                                               bool force_confirm_by_server,
                                               base::Value::Dict& response) {
  base::Value::Dict sequence_info;
  SucceedResponseFromRequestHelper(request, force_confirm_by_server, response,
                                   sequence_info);
  // Remove priority field.
  sequence_info.Remove("priority");
  response.Set("lastSucceedUploadedRecord", std::move(sequence_info));
}

void SucceedResponseFromRequestInvalidPriority(const base::Value::Dict& request,
                                               bool force_confirm_by_server,
                                               base::Value::Dict& response) {
  base::Value::Dict sequence_info;
  SucceedResponseFromRequestHelper(request, force_confirm_by_server, response,
                                   sequence_info);
  sequence_info.Remove("priority");
  // Set priority field to an invalid value.
  sequence_info.Set("priority", "abc");
  response.Set("lastSucceedUploadedRecord", std::move(sequence_info));
}

// Immitates the server response for failed record upload. Since additional
// steps and tests require the response from the server to be accurate, ASSERTS
// that the |request| must be valid, and on a valid request updates |response|.
void FailedResponseFromRequest(const base::Value::Dict& request,
                               base::Value::Dict& response) {
  base::Value::Dict seq_info;
  RetrieveFinalSequenceInformation(request, seq_info);

  response.Set("lastSucceedUploadedRecord", seq_info.Clone());
  // The lastSucceedUploadedRecord should be the record before the one indicated
  // in seq_info. |seq_info| has been built by RetrieveFinalSequenceInforamation
  // and is guaranteed to have this key.
  int64_t sequencing_id;
  ASSERT_TRUE(base::StringToInt64(*seq_info.FindString("sequencingId"),
                                  &sequencing_id));
  // The lastSucceedUploadedRecord should be the record before the one
  // indicated in seq_info.
  response.SetByDottedPath("lastSucceedUploadedRecord.sequencingId",
                           base::NumberToString(sequencing_id - 1u));

  // The firstFailedUploadedRecord.failedUploadedRecord should be the one
  // indicated in seq_info.
  response.SetByDottedPath("firstFailedUploadedRecord.failedUploadedRecord",
                           std::move(seq_info));

  auto encryption_settings_result = BuildEncryptionSettingsFromRequest(request);
  if (encryption_settings_result.has_value()) {
    response.Set("encryptionSettings",
                 std::move(encryption_settings_result.value()));
  }
}

class RecordHandlerImplTest : public ::testing::TestWithParam<
                                  ::testing::tuple</*need_encryption_key*/ bool,
                                                   /*force_confirm*/ bool>> {
 public:
  RecordHandlerImplTest()
      : client_(std::make_unique<policy::MockCloudPolicyClient>()) {}

 protected:
  void SetUp() override {
    client_->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
  }

  bool need_encryption_key() const { return std::get<0>(GetParam()); }

  bool force_confirm() const { return std::get<1>(GetParam()); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

std::unique_ptr<std::vector<EncryptedRecord>> BuildTestRecordsVector(
    int64_t number_of_test_records,
    int64_t generation_id) {
  std::unique_ptr<std::vector<EncryptedRecord>> test_records =
      std::make_unique<std::vector<EncryptedRecord>>();
  test_records->reserve(number_of_test_records);
  for (int64_t i = 0; i < number_of_test_records; i++) {
    EncryptedRecord encrypted_record;
    encrypted_record.set_encrypted_wrapped_record(
        base::StrCat({"Record Number ", base::NumberToString(i)}));
    auto* sequence_information =
        encrypted_record.mutable_sequence_information();
    sequence_information->set_generation_id(generation_id);
    sequence_information->set_sequencing_id(i);
    sequence_information->set_priority(Priority::IMMEDIATE);
    test_records->push_back(std::move(encrypted_record));
  }
  return test_records;
}

TEST_P(RecordHandlerImplTest, ForwardsRecordsToCloudPolicyClient) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  const auto force_confirm_by_server = force_confirm();

  DmServerUploadService::SuccessfulUploadResponse expected_response{
      .sequence_information = test_records->back().sequence_information(),
      .force_confirm = force_confirm()};

  EXPECT_CALL(*client_, UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_by_server](
                     base::Value::Dict request,
                     policy::CloudPolicyClient::ResponseCallback callback) {
            base::Value::Dict response;
            SucceedResponseFromRequest(request, force_confirm_by_server,
                                       response);
            std::move(callback).Run(std::move(response));
          })));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<DmServerUploadService::CompletionResponse> responder_event;

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        responder_event.cb(),
                        encryption_key_attached_event.cb());
  if (need_encryption_key()) {
    EXPECT_THAT(
        encryption_key_attached_event.result(),
        AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                       Not(IsEmpty())),
              Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
              Property(&SignedEncryptionInfo::signature, Not(IsEmpty()))));
  }
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

TEST_P(RecordHandlerImplTest, MissingPriorityField) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  const auto force_confirm_by_server = force_confirm();

  EXPECT_CALL(*client_, UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_by_server](
                     base::Value::Dict request,
                     policy::CloudPolicyClient::ResponseCallback callback) {
            base::Value::Dict response;
            SucceedResponseFromRequestMissingPriority(
                request, force_confirm_by_server, response);
            std::move(callback).Run(std::move(response));
          })));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<DmServerUploadService::CompletionResponse> responder_event;

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        responder_event.cb(),
                        encryption_key_attached_event.cb());

  auto response = responder_event.result();
  EXPECT_EQ(response.status().error_code(), error::INTERNAL);
}

TEST_P(RecordHandlerImplTest, InvalidPriorityField) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  const auto force_confirm_by_server = force_confirm();

  EXPECT_CALL(*client_,
              UploadEncryptedReport(
                  RequestValidityMatcherBuilder<>::CreateDataUpload()
                      .RemoveMatcher("sequence-information-record-matcher")
                      .Build(),
                  _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_by_server](
                     base::Value::Dict request,
                     policy::CloudPolicyClient::ResponseCallback callback) {
            base::Value::Dict response;
            SucceedResponseFromRequestInvalidPriority(
                request, force_confirm_by_server, response);
            std::move(callback).Run(std::move(response));
          })));

  test::TestEvent<SignedEncryptionInfo> encryption_key_attached_event;
  test::TestEvent<DmServerUploadService::CompletionResponse> responder_event;

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        responder_event.cb(),
                        encryption_key_attached_event.cb());

  auto response = responder_event.result();
  EXPECT_EQ(response.status().error_code(), error::INTERNAL);
}

TEST_P(RecordHandlerImplTest, ReportsUploadFailure) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  EXPECT_CALL(*client_, UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(WithArgs<2>(Invoke(
          [](base::OnceCallback<void(absl::optional<base::Value::Dict>)>
                 callback) { std::move(callback).Run(absl::nullopt); })));

  test::TestEvent<DmServerUploadService::CompletionResponse> response_event;

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);

  auto encryption_key_attached_callback =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        response_event.cb(), encryption_key_attached_callback);

  const auto response = response_event.result();
  EXPECT_THAT(response,
              Property(&DmServerUploadService::CompletionResponse::status,
                       Property(&Status::error_code, Eq(error::INTERNAL))));
}

TEST_P(RecordHandlerImplTest, UploadsGapRecordOnServerFailure) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  const auto force_confirm_by_server = force_confirm();

  const DmServerUploadService::SuccessfulUploadResponse expected_response{
      .sequence_information =
          (*test_records)[kNumTestRecords - 1].sequence_information(),
      .force_confirm = force_confirm()};

  // Once for failure, and once for gap.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*client_,
                UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
        .WillOnce(WithArgs<0, 2>(
            Invoke([](base::Value::Dict request,
                      policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value::Dict response;
              FailedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
            })));
    // TODO(b/214040998: Add a matcher for the gap upload)
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(
            Invoke([&force_confirm_by_server](
                       base::Value::Dict request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value::Dict response;
              SucceedResponseFromRequest(request, force_confirm_by_server,
                                         response);
              std::move(callback).Run(std::move(response));
            })));
  }

  test::TestEvent<DmServerUploadService::CompletionResponse> response_event;

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  EXPECT_CALL(
      encryption_key_attached,
      Call(AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                          Not(IsEmpty())),
                 Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
                 Property(&SignedEncryptionInfo::signature, Not(IsEmpty())))))
      .Times(need_encryption_key() ? 1 : 0);
  auto encryption_key_attached_callback =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        response_event.cb(), encryption_key_attached_callback);

  const auto response = response_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

// There may be cases where the server and the client do not align in the
// expected response, clients shouldn't crash in these instances, but simply
// report an internal error.
TEST_P(RecordHandlerImplTest, HandleUnknownResponseFromServer) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  EXPECT_CALL(*client_, UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(WithArgs<2>(
          Invoke([](policy::CloudPolicyClient::ResponseCallback callback) {
            std::move(callback).Run(base::Value::Dict());
          })));

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  test::TestEvent<DmServerUploadService::CompletionResponse> response_event;

  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);

  auto encryption_key_attached_callback =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        response_event.cb(), encryption_key_attached_callback);

  const auto response = response_event.result();
  EXPECT_THAT(response,
              Property(&DmServerUploadService::CompletionResponse::status,
                       Property(&Status::error_code, Eq(error::INTERNAL))));
}

TEST_P(RecordHandlerImplTest, AssignsRequestIdForRecordUploads) {
  static constexpr int64_t kNumTestRecords = 1;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  const auto force_confirm_by_server = force_confirm();

  DmServerUploadService::SuccessfulUploadResponse expected_response{
      .sequence_information = test_records->back().sequence_information(),
      .force_confirm = force_confirm()};

  EXPECT_CALL(*client_, UploadEncryptedReport(IsDataUploadRequestValid(), _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_by_server](
                     base::Value::Dict request,
                     policy::CloudPolicyClient::ResponseCallback response_cb) {
            // Validate request id is set
            auto* request_id = request.FindString(
                UploadEncryptedReportingRequestBuilder::kRequestId);
            ASSERT_NE(request_id, nullptr);
            EXPECT_THAT(*request_id, Not(IsEmpty()));

            // Trigger response callback to complete flow
            base::Value::Dict response;
            SucceedResponseFromRequest(request, force_confirm_by_server,
                                       response);
            std::move(response_cb).Run(std::move(response));
          })));

  test::TestEvent<DmServerUploadService::CompletionResponse> responder_event;
  RecordHandlerImpl handler(client_.get());
  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        responder_event.cb(), base::DoNothing());

  // We need to wait until the upload operation is marked complete (after it
  // triggers the response callback) so we can avoid leaking unmanaged
  // resources.
  auto response = responder_event.result();
  EXPECT_THAT(response, ResponseEquals(expected_response));
}

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    RecordHandlerImplTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
