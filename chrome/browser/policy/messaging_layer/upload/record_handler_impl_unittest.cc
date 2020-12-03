// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/record_handler_impl.h"

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
using ::testing::Return;
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
  explicit TestCallbackWaiterWithCounter(int counter_limit)
      : counter_limit_(counter_limit) {}

  void Signal() override {
    DCHECK_GT(counter_limit_, 0);
    if (--counter_limit_ == 0) {
      run_loop_.Quit();
    }
  }

 private:
  std::atomic<int> counter_limit_;
};

class TestCompletionResponder {
 public:
  MOCK_METHOD(void,
              RecordsHandled,
              (DmServerUploadService::CompletionResponse));
};

class RecordHandlerImplTest : public testing::Test {
 public:
  RecordHandlerImplTest()
      : client_(std::make_unique<policy::MockCloudPolicyClient>()) {}

 protected:
  void SetUp() override {
    client_->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
  }
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

std::unique_ptr<std::vector<EncryptedRecord>> RecordListBuilder(
    uint64_t number_of_test_records,
    uint64_t generation_id) {
  std::unique_ptr<std::vector<EncryptedRecord>> test_records =
      std::make_unique<std::vector<EncryptedRecord>>();

  for (uint64_t i = 0; i < number_of_test_records; i++) {
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

TEST_F(RecordHandlerImplTest, ForwardsRecordsToCloudPolicyClient) {
  uint64_t kNumTestRecords = 10;
  uint64_t kGenerationId = 1234;
  auto test_records = RecordListBuilder(kNumTestRecords, kGenerationId);

  TestCallbackWaiterWithCounter client_waiter{kNumTestRecords};
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .Times(kNumTestRecords)
      .WillRepeatedly(WithArgs<2>(
          Invoke([&client_waiter](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
            client_waiter.Signal();
          })));

  RecordHandlerImpl handler(client_.get());

  TestCallbackWaiter responder_waiter;
  TestCompletionResponder responder;
  EXPECT_CALL(responder, RecordsHandled(ValueEqualsProto(
                             test_records->back().sequencing_information())))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

  auto responder_callback = base::BindOnce(
      &TestCompletionResponder::RecordsHandled, base::Unretained(&responder));

  handler.HandleRecords(std::move(test_records), std::move(responder_callback));

  client_waiter.Wait();
  responder_waiter.Wait();
}

TEST_F(RecordHandlerImplTest, ReportsEarlyFailure) {
  uint64_t kNumSuccessfulUploads = 5;
  uint64_t kNumTestRecords = 10;
  uint64_t kGenerationId = 1234;
  auto test_records = RecordListBuilder(kNumTestRecords, kGenerationId);

  // Wait kNumSuccessfulUploads times + 1 for the failure.
  TestCallbackWaiterWithCounter client_waiter{kNumSuccessfulUploads + 1};

  ::testing::InSequence seq;
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .Times(kNumSuccessfulUploads)
      .WillRepeatedly(WithArgs<2>(
          Invoke([&client_waiter](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
            client_waiter.Signal();
          })));
  EXPECT_CALL(*client_, UploadEncryptedReport(_, _, _))
      .WillOnce(WithArgs<2>(
          Invoke([&client_waiter](base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
            client_waiter.Signal();
          })));

  RecordHandlerImpl handler(client_.get());

  TestCallbackWaiter responder_waiter;
  TestCompletionResponder responder;
  EXPECT_CALL(
      responder,
      RecordsHandled(ValueEqualsProto(
          (*test_records)[kNumSuccessfulUploads - 1].sequencing_information())))
      .WillOnce(Invoke([&responder_waiter]() { responder_waiter.Signal(); }));

  auto responder_callback = base::BindOnce(
      &TestCompletionResponder::RecordsHandled, base::Unretained(&responder));

  handler.HandleRecords(std::move(test_records), std::move(responder_callback));

  client_waiter.Wait();
  responder_waiter.Wait();
}

}  // namespace
}  // namespace reporting
