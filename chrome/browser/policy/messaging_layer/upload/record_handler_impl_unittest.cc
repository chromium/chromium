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
#include "base/task/task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "chrome/browser/policy/messaging_layer/upload/record_upload_request_builder.h"
#include "chrome/browser/policy/messaging_layer/util/test_request_payload.h"
#include "chrome/browser/policy/messaging_layer/util/test_response_payload.h"
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
      .WillOnce(MakeUploadEncryptedReportAction(std::move(
          ResponseBuilder().SetForceConfirm(force_confirm_by_server))));

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
            auto response = ResponseBuilder(std::move(request))
                                .SetForceConfirm(force_confirm_by_server)
                                .Build();
            response->RemoveByDottedPath("lastSucceedUploadedRecord.priority");
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
            auto response = ResponseBuilder(std::move(request))
                                .SetForceConfirm(force_confirm_by_server)
                                .Build();
            response->SetByDottedPath("lastSucceedUploadedRecord.priority",
                                      "abc");
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

TEST_P(RecordHandlerImplTest, MissingSequenceInformation) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  // test records that has one record with missing sequence information.
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);
  test_records->back().clear_sequence_information();

  // The response should show an error and UploadEncryptedReport should not have
  // been even called, because UploadEncryptedReportingRequestBuilder::Build()
  // should fail in this situation.
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _)).Times(0);

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
      .WillOnce(MakeUploadEncryptedReportAction(
          std::move(ResponseBuilder().SetNull(true))));

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
        .WillOnce(MakeUploadEncryptedReportAction(
            std::move(ResponseBuilder().SetSuccess(false))));
    EXPECT_CALL(*client_,
                UploadEncryptedReport(IsGapUploadRequestValid(), _, _))
        .WillOnce(MakeUploadEncryptedReportAction(std::move(
            ResponseBuilder().SetForceConfirm(force_confirm_by_server))));
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
      .WillOnce(MakeUploadEncryptedReportAction(std::move(
          ResponseBuilder().SetForceConfirm(force_confirm_by_server))));

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
