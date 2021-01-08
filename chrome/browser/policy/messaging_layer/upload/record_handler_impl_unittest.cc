// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

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
#include "chrome/browser/policy/messaging_layer/util/status.h"
#include "chrome/browser/policy/messaging_layer/util/status_macros.h"
#include "chrome/browser/policy/messaging_layer/util/statusor.h"
#include "chrome/browser/policy/messaging_layer/util/task_runner_context.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/proto/record.pb.h"
#include "components/policy/proto/record_constants.pb.h"
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

MATCHER_P(ValueEqualsProto,
          expected,
          "Compares StatusOr<MessageLite> to expected MessageLite") {
  if (!arg.ok()) {
    return false;
  }
  if (arg.ValueOrDie().GetTypeName() != expected.GetTypeName()) {
    return false;
  }
  return arg.ValueOrDie().SerializeAsString() == expected.SerializeAsString();
}

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() = default;

  virtual void Signal() { run_loop_.Quit(); }

  void Wait() { run_loop_.Run(); }

 protected:
  base::RunLoop run_loop_;
};

class TestCallbackWaiterWithCounter : public TestCallbackWaiter {
 public:
  explicit TestCallbackWaiterWithCounter(size_t counter_limit)
      : counter_limit_(counter_limit) {}

  void Signal() override {
    DCHECK_GT(counter_limit_, 0u);
    if (--counter_limit_ == 0u) {
      run_loop_.Quit();
    }
  }

 private:
  std::atomic<size_t> counter_limit_;
};

using TestCompletionResponder =
    MockFunction<void(DmServerUploadService::CompletionResponse)>;

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
  const auto* seq_info = encrypted_record_list->GetList().rbegin()->FindDictKey(
      "sequencingInformation");
  ASSERT_TRUE(seq_info != nullptr);
  ASSERT_TRUE(!seq_info->FindStringKey("sequencingId")->empty());
  ASSERT_TRUE(!seq_info->FindStringKey("generationId")->empty());
  ASSERT_TRUE(seq_info->FindIntKey("priority"));

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
                                base::Value& response) {
  base::Value seq_info{base::Value::Type::DICTIONARY};
  RetrieveFinalSequencingInformation(request, seq_info);
  response.SetPath("lastSucceedUploadedRecord", std::move(seq_info));

  // If attach_encryption_settings it true, process that.
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
  uint64_t sequencing_id;
  ASSERT_TRUE(base::StringToUint64(*seq_info.FindStringKey("sequencingId"),
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

class RecordHandlerImplTest : public ::testing::TestWithParam<bool> {
 public:
  RecordHandlerImplTest()
      : client_(std::make_unique<policy::MockCloudPolicyClient>()) {}

 protected:
  void SetUp() override {
    client_->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
  }

  bool need_encryption_key() const { return GetParam(); }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

std::unique_ptr<std::vector<EncryptedRecord>> BuildTestRecordsVector(
    size_t number_of_test_records,
    uint64_t generation_id) {
  std::unique_ptr<std::vector<EncryptedRecord>> test_records =
      std::make_unique<std::vector<EncryptedRecord>>();
  test_records->reserve(number_of_test_records);
  for (size_t i = 0; i < number_of_test_records; i++) {
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
  constexpr size_t kNumTestRecords = 10;
  constexpr uint64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  TestCallbackWaiterWithCounter client_waiter{kNumTestRecords};
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(WithArgs<0, 2>(
          Invoke([&client_waiter](
                     base::Value request,
                     policy::CloudPolicyClient::ResponseCallback callback) {
            base::Value response{base::Value::Type::DICTIONARY};
            SucceedResponseFromRequest(request, response);
            std::move(callback).Run(std::move(response));
            client_waiter.Signal();
          })));

  RecordHandlerImpl handler(client_.get());

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  StrictMock<TestCompletionResponder> responder;
  TestCallbackWaiter responder_waiter;

  EXPECT_CALL(
      encryption_key_attached,
      Call(AllOf(Property(&SignedEncryptionInfo::public_asymmetric_key,
                          Not(IsEmpty())),
                 Property(&SignedEncryptionInfo::public_key_id, Gt(0)),
                 Property(&SignedEncryptionInfo::signature, Not(IsEmpty())))))
      .Times(need_encryption_key() ? 1 : 0);

  EXPECT_CALL(
      responder,
      Call(ValueEqualsProto(test_records->back().sequencing_information())))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

  auto encryption_key_attached_callback =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  auto responder_callback = base::BindOnce(&TestCompletionResponder::Call,
                                           base::Unretained(&responder));

  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        std::move(responder_callback),
                        encryption_key_attached_callback);

  client_waiter.Wait();
  responder_waiter.Wait();
}

TEST_P(RecordHandlerImplTest, ReportsEarlyFailure) {
  uint64_t kNumSuccessfulUploads = 5;
  uint64_t kNumTestRecords = 10;
  uint64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  // Wait kNumSuccessfulUploads times + 1 for the failure.
  TestCallbackWaiterWithCounter client_waiter{kNumSuccessfulUploads + 1};

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .Times(kNumSuccessfulUploads)
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([&client_waiter](
                       base::Value request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
              SucceedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
              client_waiter.Signal();
            })));
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<2>(Invoke(
            [&client_waiter](base::OnceCallback<void(
                                 base::Optional<base::Value>)> callback) {
              std::move(callback).Run(base::nullopt);
              client_waiter.Signal();
            })));
  }
  RecordHandlerImpl handler(client_.get());

  StrictMock<TestCompletionResponder> responder;
  TestCallbackWaiter responder_waiter;
  EXPECT_CALL(
      responder,
      Call(ValueEqualsProto(
          (*test_records)[kNumSuccessfulUploads - 1].sequencing_information())))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

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

  auto responder_callback = base::BindOnce(&TestCompletionResponder::Call,
                                           base::Unretained(&responder));

  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        std::move(responder_callback),
                        encryption_key_attached_callback);

  client_waiter.Wait();
  responder_waiter.Wait();
}

TEST_P(RecordHandlerImplTest, UploadsGapRecordOnServerFailure) {
  uint64_t kNumInitialSuccessfulUploads = 5;
  uint64_t kNumTestRecords = 10;
  uint64_t kNumFinalSuccessfulUploads =
      kNumTestRecords - kNumInitialSuccessfulUploads;
  uint64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  // Wait kNumTestRecords times + 1 for the failure.
  TestCallbackWaiterWithCounter client_waiter{kNumTestRecords + 1};

  {
    ::testing::InSequence seq;
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .Times(kNumInitialSuccessfulUploads)
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([&client_waiter](
                       base::Value request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
              SucceedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
              client_waiter.Signal();
            })));
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .WillOnce(WithArgs<0, 2>(
            Invoke([&client_waiter](
                       base::Value request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
              FailedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
              client_waiter.Signal();
            })));
    EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
        .Times(kNumFinalSuccessfulUploads)
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([&client_waiter](
                       base::Value request,
                       policy::CloudPolicyClient::ResponseCallback callback) {
              base::Value response{base::Value::Type::DICTIONARY};
              SucceedResponseFromRequest(request, response);
              std::move(callback).Run(std::move(response));
              client_waiter.Signal();
            })));
  }

  RecordHandlerImpl handler(client_.get());

  StrictMock<TestCallbackWaiter> responder_waiter;
  TestCompletionResponder responder;
  EXPECT_CALL(
      responder,
      Call(ValueEqualsProto(
          (*test_records)[kNumTestRecords - 1].sequencing_information())))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

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
  auto responder_callback = base::BindOnce(&TestCompletionResponder::Call,
                                           base::Unretained(&responder));

  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        std::move(responder_callback),
                        encryption_key_attached_callback);

  client_waiter.Wait();
  responder_waiter.Wait();
}

// There may be cases where the server and the client do not align in the
// expected response, clients shouldn't crash in these instances, but simply
// report an internal error.
TEST_P(RecordHandlerImplTest, HandleUnknownResponseFromServer) {
  constexpr size_t kNumTestRecords = 10;
  constexpr uint64_t kGenerationId = 1234;
  auto test_records = BuildTestRecordsVector(kNumTestRecords, kGenerationId);

  TestCallbackWaiterWithCounter client_waiter{kNumTestRecords};
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(WithArgs<2>(
          Invoke([&client_waiter](
                     policy::CloudPolicyClient::ResponseCallback callback) {
            std::move(callback).Run(base::Value{base::Value::Type::DICTIONARY});
            client_waiter.Signal();
          })));

  RecordHandlerImpl handler(client_.get());

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  StrictMock<TestCompletionResponder> responder;
  TestCallbackWaiter responder_waiter;

  EXPECT_CALL(encryption_key_attached, Call(_)).Times(0);

  EXPECT_CALL(
      responder,
      Call(Property(&StatusOr<SequencingInformation>::status,
                    Property(&Status::error_code, Eq(error::INTERNAL)))))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

  auto encryption_key_attached_callback =
      base::BindRepeating(&TestEncryptionKeyAttached::Call,
                          base::Unretained(&encryption_key_attached));

  auto responder_callback = base::BindOnce(&TestCompletionResponder::Call,
                                           base::Unretained(&responder));

  handler.HandleRecords(need_encryption_key(), std::move(test_records),
                        std::move(responder_callback),
                        encryption_key_attached_callback);

  client_waiter.Wait();
  responder_waiter.Wait();
}

INSTANTIATE_TEST_SUITE_P(NeedOrNoNeedKey,
                         RecordHandlerImplTest,
                         testing::Bool());
}  // namespace
}  // namespace reporting
