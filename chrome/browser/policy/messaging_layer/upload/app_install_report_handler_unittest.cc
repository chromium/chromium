// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/app_install_report_handler.h"

#include "base/json/json_writer.h"
#include "base/optional.h"
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

MATCHER_P(MatchValue, expected, "matches base::Value") {
  std::string arg_string;
  if (!base::JSONWriter::Write(arg, &arg_string)) {
    LOG(INFO) << "Unable to serialize the arg";
    return false;
  }

  DCHECK(expected);
  std::string expected_string;
  if (!base::JSONWriter::Write(*expected, &expected_string)) {
    LOG(INFO) << "Unable to serialize the expected";
    return false;
  }

  return arg_string == expected_string;
}

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {}

  virtual void Signal() { run_loop_->Quit(); }

  void Wait() { run_loop_->Run(); }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class AppInstallReportHandlerTest : public testing::Test {
 public:
  AppInstallReportHandlerTest()
      : client_(std::make_unique<policy::MockCloudPolicyClient>()) {}

 protected:
  void SetUp() override {
    client_->SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
  }
  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<policy::MockCloudPolicyClient> client_;
};

class TestRecord : public Record {
 public:
  explicit TestRecord(base::StringPiece key = "TEST_KEY",
                      base::StringPiece value = "TEST_VALUE") {
    data_.SetKey(key, base::Value(value));
    std::string json_data;
    base::JSONWriter::Write(data_, &json_data);

    set_data(json_data);
    set_destination(Destination::UPLOAD_EVENTS);
  }

  const base::Value* data() const { return &data_; }

 private:
  base::Value data_{base::Value::Type::DICTIONARY};
};

TEST_F(AppInstallReportHandlerTest, AcceptsValidRecord) {
  TestCallbackWaiter waiter;
  TestRecord test_record;
  EXPECT_CALL(*client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillOnce(WithArgs<1>(
          Invoke([&waiter](AppInstallReportHandler::ClientCallback& callback) {
            std::move(callback).Run(true);
            waiter.Signal();
          })));

  AppInstallReportHandler handler(client_.get());
  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_OK(handle_status);
  waiter.Wait();
}

TEST_F(AppInstallReportHandlerTest, DeniesInvalidDestination) {
  EXPECT_CALL(*client_, UploadExtensionInstallReport_(_, _)).Times(0);
  AppInstallReportHandler handler(client_.get());

  TestRecord test_record;
  test_record.set_destination(Destination::MEET_DEVICE_TELEMETRY);

  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_FALSE(handle_status.ok());
  EXPECT_EQ(handle_status.error_code(), error::INVALID_ARGUMENT);
}

TEST_F(AppInstallReportHandlerTest, DeniesInvalidData) {
  EXPECT_CALL(*client_, UploadExtensionInstallReport_(_, _)).Times(0);
  AppInstallReportHandler handler(client_.get());

  TestRecord test_record;
  test_record.set_data("BAD_DATA");
  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_FALSE(handle_status.ok());
  EXPECT_EQ(handle_status.error_code(), error::INVALID_ARGUMENT);
}

TEST_F(AppInstallReportHandlerTest, ReportsUnsuccessfulCall) {
  TestCallbackWaiter waiter;

  TestRecord test_record;
  EXPECT_CALL(*client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillOnce(WithArgs<1>(
          Invoke([&waiter](AppInstallReportHandler::ClientCallback& callback) {
            std::move(callback).Run(false);
            waiter.Signal();
          })));

  AppInstallReportHandler handler(client_.get());
  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_OK(handle_status);
  waiter.Wait();
}

class TestCallbackWaiterWithCounter : public TestCallbackWaiter {
 public:
  explicit TestCallbackWaiterWithCounter(int counter_limit)
      : counter_limit_(counter_limit) {}

  void Signal() override {
    DCHECK_GT(counter_limit_, 0);
    if (--counter_limit_ == 0) {
      run_loop_->Quit();
    }
  }

 private:
  std::atomic<int> counter_limit_;
};

TEST_F(AppInstallReportHandlerTest, AcceptsMultipleValidRecords) {
  const int kExpectedCallTimes = 10;
  TestCallbackWaiterWithCounter waiter{kExpectedCallTimes};

  TestRecord test_record;
  EXPECT_CALL(*client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillRepeatedly(WithArgs<1>(
          Invoke([&waiter](AppInstallReportHandler::ClientCallback& callback) {
            std::move(callback).Run(true);
            waiter.Signal();
          })));

  AppInstallReportHandler handler(client_.get());

  for (int i = 0; i < kExpectedCallTimes; i++) {
    Status handle_status = handler.HandleRecord(test_record);
    EXPECT_OK(handle_status);
  }
  waiter.Wait();
}

}  // namespace
}  // namespace reporting
