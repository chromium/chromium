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
using ::testing::Invoke;
using ::testing::MockFunction;
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

// Helper function composes JSON represented as base::Value from Sequencing
// information in request.
base::Value ValueFromSucceededSequencingInfo(
    const base::Optional<base::Value> request) {
  EXPECT_TRUE(request.has_value());
  EXPECT_TRUE(request.value().is_dict());
  base::Value response(base::Value::Type::DICTIONARY);

  // Retrieve and process sequencing information
  const base::Value* const encrypted_record_list =
      request.value().FindListKey("encryptedRecord");
  EXPECT_TRUE(encrypted_record_list != nullptr);
  EXPECT_FALSE(encrypted_record_list->GetList().empty());
  const base::Value* seq_info =
      encrypted_record_list->GetList().rbegin()->FindDictKey(
          "sequencingInformation");
  EXPECT_TRUE(seq_info != nullptr);
  response.SetPath("lastSucceedUploadedRecord", seq_info->Clone());

  // If attach_encryption_settings it true, process that.
  const auto attach_encryption_settings =
      request.value().FindBoolKey("attachEncryptionSettings");
  if (attach_encryption_settings.has_value() &&
      attach_encryption_settings.value()) {
    base::Value encryption_settings{base::Value::Type::DICTIONARY};
    std::string public_key;
    base::Base64Encode("PUBLIC KEY", &public_key);
    encryption_settings.SetStringKey("publicKey", public_key);
    encryption_settings.SetIntKey("publicKeyId", 12345);
    std::string public_key_signature;
    // TODO(b/170054326): Generate signature.
    base::Base64Encode("PUBLIC KEY SIG", &public_key_signature);
    encryption_settings.SetStringKey("publicKeySignature",
                                     public_key_signature);
    response.SetPath("encryptionSettings", std::move(encryption_settings));
  }

  return response;
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
            std::move(callback).Run(
                ValueFromSucceededSequencingInfo(std::move(request)));
            client_waiter.Signal();
          })));

  RecordHandlerImpl handler(client_.get());

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  StrictMock<TestCompletionResponder> responder;
  TestCallbackWaiter responder_waiter;

  EXPECT_CALL(encryption_key_attached, Call(_))
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
              std::move(callback).Run(
                  ValueFromSucceededSequencingInfo(std::move(request)));
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

  StrictMock<TestEncryptionKeyAttached> encryption_key_attached;
  StrictMock<TestCompletionResponder> responder;
  TestCallbackWaiter responder_waiter;

  EXPECT_CALL(encryption_key_attached, Call(_))
      .Times(need_encryption_key() ? 1 : 0);

  EXPECT_CALL(
      responder,
      Call(ValueEqualsProto(
          (*test_records)[kNumSuccessfulUploads - 1].sequencing_information())))
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
