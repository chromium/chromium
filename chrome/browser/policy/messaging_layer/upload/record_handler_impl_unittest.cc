// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

#include <tuple>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/optional.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/policy/messaging_layer/upload/dm_server_upload_service.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/task_runner_context.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  if (arg.ValueOrDie().sequencing_information.GetTypeName() !=
      expected.sequencing_information.GetTypeName()) {
    return false;
  }
  if (arg.ValueOrDie().sequencing_information.SerializeAsString() !=
      expected.sequencing_information.SerializeAsString()) {
    return false;
  }
  return arg.ValueOrDie().force_confirm == expected.force_confirm;
}

using TestEncryptionKeyAttached = MockFunction<void(SignedEncryptionInfo)>;

// Helper function for retrieving and processing the SequencingInformation from
// a request.
void RetrieveFinalSequencingInformation(const base::Value& request,
                                        base::Value& sequencing_info) {
  ASSERT_TRUE(request.is_dict());

  // Retrieve and process sequencing information
  const base::Value* const encrypted_record_list =
      request.FindListKey("encryptedRecord");
  ASSERT_TRUE(encrypted_record_list != nullptr);
  ASSERT_FALSE(encrypted_record_list->GetList().empty());
  const auto* const seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequenceInformation");
  ASSERT_TRUE(seq_info != nullptr);
  ASSERT_TRUE(!seq_info->FindStringKey("sequencingId")->empty());
  ASSERT_TRUE(!seq_info->FindStringKey("generationId")->empty());
  ASSERT_TRUE(seq_info->FindIntKey("priority"));
  // For backwards compatibility, we must also have unsigned sequencing info.
  const auto* const unsigned_seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequencingInformation");
  ASSERT_TRUE(unsigned_seq_info != nullptr);
  ASSERT_TRUE(!unsigned_seq_info->FindStringKey("sequencingId")->empty());
  ASSERT_TRUE(!unsigned_seq_info->FindStringKey("generationId")->empty());
  ASSERT_TRUE(unsigned_seq_info->FindIntKey("priority"));

  sequencing_info.MergeDictionary(seq_info);
  // Set half of sequencing information to return a string instead of an int for
  // priority.
  int64_t sequencing_id;
  ASSERT_TRUE(base::StringToInt64(
      *sequencing_info.FindStringKey("sequencingId"), &sequencing_id));
  if (sequencing_id % 2) {
    const auto int_result = sequencing_info.FindIntKey("priority");
    ASSERT_TRUE(int_result.has_value());
    sequencing_info.RemoveKey("priority");
    sequencing_info.SetStringKey("priority", Priority_Name(int_result.value()));
  }
}

base::Optional<base::Value> BuildEncryptionSettingsFromRequest(
    const base::Value& request) {
  // If attach_encryption_settings it true, process that.
  const auto attach_encryption_settings =
      request.FindBoolKey("attachEncryptionSettings");
  if (!attach_encryption_settings.has_value() ||
      !attach_encryption_settings.value()) {
    return base::nullopt;
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
void SucceedResponseFromRequest(const base::Value& request,
                                bool force_confirm_by_server,
                                base::Value& response) {
  base::Value seq_info{base::Value::Type::DICTIONARY};
  RetrieveFinalSequencingInformation(request, seq_info);
  response.SetPath("lastSucceedUploadedRecord", std::move(seq_info));

  // If force_confirm is true, process that.
  if (force_confirm_by_server) {
    response.SetPath("forceConfirm", base::Value(true));
  }

  // If attach_encryption_settings is true, process that.
  auto encryption_settings_result = BuildEncryptionSettingsFromRequest(request);
  if (encryption_settings_result.has_value()) {
    response.SetPath("encryptionSettings",
                     std::move(encryption_settings_result.value()));
  }
}

// Immitates the server response for failed record upload. Since additional
// steps and tests require the response from the server to be accurate, ASSERTS
// that the |request| must be valid, and on a valid request updates |response|.
void FailedResponseFromRequest(const base::Value& request,
                               base::Value& response) {
  base::Value seq_info{base::Value::Type::DICTIONARY};
  RetrieveFinalSequencingInformation(request, seq_info);

  response.SetPath("lastSucceedUploadedRecord", seq_info.Clone());
  // The lastSucceedUploadedRecord should be the record before the one indicated
  // in seq_info. |seq_info| has been built by
  // RetrieveFinalSequencingInforamation and is guaranteed to have this key.
  int64_t sequencing_id;
  ASSERT_TRUE(base::StringToInt64(*seq_info.FindStringKey("sequencingId"),
                                  &sequencing_id));
  // The lastSucceedUploadedRecord should be the record before the one
  // indicated in seq_info.
  response.SetStringPath("lastSucceedUploadedRecord.sequencingId",
                         base::NumberToString(sequencing_id - 1u));

  // The firstFailedUploadedRecord.failedUploadedRecord should be the one
  // indicated in seq_info.
  response.SetPath("firstFailedUploadedRecord.failedUploadedRecord",
                   std::move(seq_info));

  auto encryption_settings_result = BuildEncryptionSettingsFromRequest(request);
  if (encryption_settings_result.has_value()) {
    response.SetPath("encryptionSettings",
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
    auto* sequencing_information =
        encrypted_record.mutable_sequencing_information();
    sequencing_information->set_generation_id(generation_id);
    sequencing_information->set_sequencing_id(i);
    sequencing_information->set_priority(Priority::IMMEDIATE);
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
      .sequencing_information = test_records->back().sequencing_information(),
      .force_confirm = force_confirm()};

  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<0, 2>(
          Invoke([&force_confirm_by_server](
                     base::Value request,
                     policy::CloudPolicyClient::ResponseCallback callback) {
            base::Value response{base::Value::Type::DICTIONARY};
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

TEST_P(RecordHandlerImplTest, ReportsUploadFailure) {
  static constexpr int64_t kNumTestRecords = 10;
  static constexpr int64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<2>(Invoke(
          [](base::OnceCallback<void(base::Optional<base::Value>)> callback) {
            std::move(callback).Run(base::nullopt);
          })));

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
      .sequencing_information =
          (*test_records)[kNumTestRecords - 1].sequencing_information(),
      .force_confirm = force_confirm()};

  // Once for failure, and once for gap.
  {
    ::testing::InSequence seq;
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(
            Invoke([](base::Value request,
                      policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
              FailedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
            })));
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(
            Invoke([&force_confirm_by_server](
                       base::Value request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
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

  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<2>(
          Invoke([](policy::CloudPolicyClient::ResponseCallback callback) {
            std::move(callback).Run(base::Value{base::Value::Type::DICTIONARY});
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

INSTANTIATE_TEST_SUITE_P(
    NeedOrNoNeedKey,
    RecordHandlerImplTest,
    ::testing::Combine(/*need_encryption_key*/ ::testing::Bool(),
                       /*force_confirm*/ ::testing::Bool()));
}  // namespace
}  // namespace reporting
