// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/messaging_layer/upload/meet_device_telemetry_report_handler.h"

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
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::WithArgs;

namespace reporting {
namespace {

MATCHER_P(MatchValue, expected, "matches base::Value") {
  base::Value* const events = arg.FindListKey("events");
  if (!events) {
    LOG(ERROR) << "Arg does not have 'events' or 'events' is not a list";
    return false;
  }
  base::Value::ListView events_list = events->GetList();
  if (events_list.size() != 1) {
    LOG(ERROR) << "'events' is empty or has more than one element in the list";
    return false;
  }
  const base::Value& event = *events_list.begin();
  const auto destination = event.FindIntKey("destination");
  if (!destination.has_value() ||
      destination.value() != Destination::MEET_DEVICE_TELEMETRY) {
    LOG(ERROR) << "'destination' is wrong or missing";
    return false;
  }
  const std::string* const data = event.FindStringKey("data");
  if (!data) {
    LOG(ERROR) << "'data' is missing";
    return false;
  }

  DCHECK(expected);
  std::string expected_string;
  if (!base::JSONWriter::Write(*expected, &expected_string)) {
    LOG(INFO) << "Unable to serialize the expected";
    return false;
  }

  return *data == expected_string;
}

class TestCallbackWaiter {
 public:
  TestCallbackWaiter() : run_loop_(std::make_unique<base::RunLoop>()) {}

  virtual void Signal() { run_loop_->Quit(); }

  void Wait() { run_loop_->Run(); }

 protected:
  std::unique_ptr<base::RunLoop> run_loop_;
};

class MeetDeviceTelemetryReportHandlerTest : public testing::Test {
 public:
  MeetDeviceTelemetryReportHandlerTest() = default;

  void SetUp() override {
    // Set up client.
    client_.SetDMToken(
        policy::DMToken::CreateValidTokenForTesting("FAKE_DM_TOKEN").value());
  }

 protected:
  content::BrowserTaskEnvironment task_envrionment_;

  policy::MockCloudPolicyClient client_;
};

class TestRecord : public Record {
 public:
  explicit TestRecord(base::StringPiece key = "TEST_KEY",
                      base::StringPiece value = "TEST_VALUE") {
    data_.SetKey(key, base::Value(value));
    std::string json_data;
    base::JSONWriter::Write(data_, &json_data);

    set_data(json_data);
    set_destination(Destination::MEET_DEVICE_TELEMETRY);
  }

  const base::Value* data() const { return &data_; }

 private:
  base::Value data_{base::Value::Type::DICTIONARY};
};

TEST_F(MeetDeviceTelemetryReportHandlerTest, AcceptsValidRecord) {
  TestCallbackWaiter waiter;
  TestRecord test_record;
  EXPECT_CALL(client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillOnce(WithArgs<1>(Invoke(
          [&waiter](
              MeetDeviceTelemetryReportHandler::ClientCallback& callback) {
            std::move(callback).Run(true);
            waiter.Signal();
          })));

  MeetDeviceTelemetryReportHandler handler(/*profile=*/nullptr, &client_);
  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_OK(handle_status);
  waiter.Wait();
}

TEST_F(MeetDeviceTelemetryReportHandlerTest, DeniesInvalidDestination) {
  EXPECT_CALL(client_, UploadExtensionInstallReport_(_, _)).Times(0);
  MeetDeviceTelemetryReportHandler handler(/*profile=*/nullptr, &client_);

  TestRecord test_record;
  test_record.set_destination(Destination::UPLOAD_EVENTS);

  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_FALSE(handle_status.ok());
  EXPECT_EQ(handle_status.error_code(), error::INVALID_ARGUMENT);
}

TEST_F(MeetDeviceTelemetryReportHandlerTest, DeniesInvalidData) {
  EXPECT_CALL(client_, UploadExtensionInstallReport_(_, _)).Times(0);
  MeetDeviceTelemetryReportHandler handler(/*profile=*/nullptr, &client_);

  TestRecord test_record;
  test_record.clear_data();
  Status handle_status = handler.HandleRecord(test_record);
  EXPECT_FALSE(handle_status.ok());
  EXPECT_EQ(handle_status.error_code(), error::INVALID_ARGUMENT);
}

TEST_F(MeetDeviceTelemetryReportHandlerTest, ReportsUnsuccessfulCall) {
  TestCallbackWaiter waiter;

  TestRecord test_record;
  EXPECT_CALL(client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillOnce(WithArgs<1>(Invoke(
          [&waiter](
              MeetDeviceTelemetryReportHandler::ClientCallback& callback) {
            std::move(callback).Run(false);
            waiter.Signal();
          })));

  MeetDeviceTelemetryReportHandler handler(/*profile=*/nullptr, &client_);
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

TEST_F(MeetDeviceTelemetryReportHandlerTest, AcceptsMultipleValidRecords) {
  const int kExpectedCallTimes = 10;
  TestCallbackWaiterWithCounter waiter{kExpectedCallTimes};

  TestRecord test_record;
  EXPECT_CALL(client_,
              UploadExtensionInstallReport_(MatchValue(test_record.data()), _))
      .WillRepeatedly(WithArgs<1>(Invoke(
          [&waiter](
              MeetDeviceTelemetryReportHandler::ClientCallback& callback) {
            std::move(callback).Run(true);
            waiter.Signal();
          })));

  MeetDeviceTelemetryReportHandler handler(/*profile=*/nullptr, &client_);

  for (int i = 0; i < kExpectedCallTimes; i++) {
    Status handle_status = handler.HandleRecord(test_record);
    EXPECT_OK(handle_status);
  }
  waiter.Wait();
}

}  // namespace
}  // namespace reporting
