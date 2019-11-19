// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Mock;
using testing::SaveArg;
using testing::WithArgs;
using testing::_;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::TimeDelta kMinRetryBackoff = base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kMaxRetryBackoff = base::TimeDelta::FromDays(1);

static const char kDmToken[] = "token";
static const char kPackageName[] = "package";

MATCHER_P(MatchValue, expected, "matches base::Value") {
  std::string arg_serialized_string;
  JSONStringValueSerializer arg_serializer(&arg_serialized_string);
  if (!arg_serializer.Serialize(arg))
    return false;

  DCHECK(expected);
  std::string expected_serialized_string;
  JSONStringValueSerializer expected_serializer(&expected_serialized_string);
  if (!expected_serializer.Serialize(*expected))
    return false;

  return arg_serialized_string == expected_serialized_string;
}

ACTION_TEMPLATE(MoveArg,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(out)) {
  *out = std::move(*testing::get<k>(args));
}

class MockAppInstallEventLogUploaderDelegate
    : public AppInstallEventLogUploader::Delegate {
 public:
  MockAppInstallEventLogUploaderDelegate() {}

  void SerializeForUpload(SerializationCallback callback) override {
    SerializeForUpload_(&callback);
  }

  MOCK_METHOD1(SerializeForUpload_, void(SerializationCallback*));
  MOCK_METHOD0(OnUploadSuccess, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAppInstallEventLogUploaderDelegate);
};

}  // namespace

class AppInstallEventLogUploaderTest : public testing::Test {
 protected:
  AppInstallEventLogUploaderTest() = default;

  void TearDown() override {
    Mock::VerifyAndClearExpectations(&client_);
    EXPECT_CALL(client_, CancelAppInstallReportUpload());
    uploader_.reset();
  }

  void RegisterClient() {
    client_.dm_token_ = kDmToken;
    client_.NotifyRegistrationStateChanged();
  }

  void UnregisterClient() {
    client_.dm_token_.clear();
    client_.NotifyRegistrationStateChanged();
  }

  void CreateUploader() {
    uploader_ = std::make_unique<AppInstallEventLogUploader>(
        &client_, /*profile=*/nullptr);
    uploader_->SetDelegate(&delegate_);
  }

  void CompleteSerialize() {
    EXPECT_CALL(delegate_, SerializeForUpload_(_))
        .WillOnce(WithArgs<0>(Invoke(
            [=](AppInstallEventLogUploader::Delegate::SerializationCallback*
                    callback) { std::move(*callback).Run(&log_); })));
  }

  void CaptureSerialize(
      AppInstallEventLogUploader::Delegate::SerializationCallback* callback) {
    EXPECT_CALL(delegate_, SerializeForUpload_(_))
        .WillOnce(MoveArg<0>(callback));
  }

  void ClearReportDict() {
    base::DictionaryValue* mutable_dict;
    if (value_report_.GetAsDictionary(&mutable_dict))
      mutable_dict->Clear();
    else
      NOTREACHED();
  }

  void CompleteUpload(bool success) {
    ClearReportDict();
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        ConvertProtoToValue(&log_, /*profile=*/nullptr),
        reporting::GetContext(/*profile=*/nullptr));

    EXPECT_CALL(client_, UploadRealtimeReport(MatchValue(&value_report_), _))
        .WillOnce(WithArgs<1>(
            Invoke([=](const CloudPolicyClient::StatusCallback& callback) {
              callback.Run(success);
            })));
  }

  void CaptureUpload(CloudPolicyClient::StatusCallback* callback) {
    ClearReportDict();
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        ConvertProtoToValue(&log_, /*profile=*/nullptr),
        reporting::GetContext(/*profile=*/nullptr));

    CloudPolicyClient::StatusCallback status_callback;
    EXPECT_CALL(client_, UploadRealtimeReport(MatchValue(&value_report_), _))
        .WillOnce(SaveArg<1>(callback));
  }

  void CompleteSerializeAndUpload(bool success) {
    CompleteSerialize();
    CompleteUpload(success);
  }

  void CompleteSerializeAndCaptureUpload(
      CloudPolicyClient::StatusCallback* callback) {
    CompleteSerialize();
    CaptureUpload(callback);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  em::AppInstallReportRequest log_;
  base::Value value_report_{base::Value::Type::DICTIONARY};

  MockCloudPolicyClient client_;
  MockAppInstallEventLogUploaderDelegate delegate_;
  std::unique_ptr<AppInstallEventLogUploader> uploader_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogUploaderTest);
};

// Make a log upload request. Have serialization and log upload succeed. Verify
// that the delegate is notified of the success.
TEST_F(AppInstallEventLogUploaderTest, RequestSerializeAndUpload) {
  RegisterClient();
  CreateUploader();

  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  uploader_->RequestUpload();
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Make a second upload request. Have the first upload succeed. Verify that the
// delegate is notified of the first request's success and no serialization is
// started for the second request.
TEST_F(AppInstallEventLogUploaderTest, RequestSerializeRequestAndUpload) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::StatusCallback status_callback;
  CompleteSerializeAndCaptureUpload(&status_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeForUpload_(_)).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnUploadSuccess());
  EXPECT_CALL(delegate_, SerializeForUpload_(_)).Times(0);
  status_callback.Run(true);
}

// Make a log upload request. Have serialization begin. Make a second upload
// request. Verify that no serialization is started for the second request.
// Then, have the first request's serialization and upload succeed. Verify that
// the delegate is notified of the first request's success.
TEST_F(AppInstallEventLogUploaderTest, RequestRequestSerializeAndUpload) {
  RegisterClient();
  CreateUploader();

  AppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeForUpload_(_)).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  CompleteUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization begin. Cancel the request. Have
// the serialization succeed. Verify that the serialization result is ignored
// and no upload is started.
TEST_F(AppInstallEventLogUploaderTest, RequestCancelAndSerialize) {
  RegisterClient();
  CreateUploader();

  AppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  uploader_->CancelUpload();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, UploadRealtimeReport(_, _)).Times(0);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Cancel the request. Verify that the upload is canceled in the client.
TEST_F(AppInstallEventLogUploaderTest, RequestSerializeAndCancel) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::StatusCallback status_callback;
  CompleteSerializeAndCaptureUpload(&status_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  uploader_->CancelUpload();
}

// Make a log upload request. Have serialization succeed but log upload fail.
// Verify that serialization and log upload are retried with exponential
// backoff. Have the retries fail until the maximum backoff is seen twice. Then,
// have serialization and log upload succeed. Verify that the delegate is
// notified of the success. Then, make another log upload request. Have the
// serialization succeed but log upload fail again. Verify that the backoff has
// returned to the minimum.
TEST_F(AppInstallEventLogUploaderTest, Retry) {
  RegisterClient();
  CreateUploader();

  CompleteSerializeAndUpload(false /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(&client_);

  const base::TimeDelta min_delay = kMinRetryBackoff;
  const base::TimeDelta max_delay = kMaxRetryBackoff;

  base::TimeDelta expected_delay = min_delay;
  int max_delay_count = 0;
  while (max_delay_count < 2) {
    EXPECT_EQ(expected_delay,
              task_environment_.NextMainThreadPendingTaskDelay());

    CompleteSerializeAndUpload(false /* success */);
    EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
    task_environment_.FastForwardBy(expected_delay);
    Mock::VerifyAndClearExpectations(&delegate_);
    Mock::VerifyAndClearExpectations(&client_);

    if (expected_delay == max_delay) {
      ++max_delay_count;
    }
    expected_delay = std::min(expected_delay * 2, max_delay);
  }

  EXPECT_EQ(expected_delay, task_environment_.NextMainThreadPendingTaskDelay());

  log_.add_app_install_reports()->set_package(kPackageName);
  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  task_environment_.FastForwardBy(expected_delay);
  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(&client_);

  CompleteSerializeAndUpload(false /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  uploader_->RequestUpload();

  EXPECT_EQ(min_delay, task_environment_.NextMainThreadPendingTaskDelay());
}

// Create the uploader using a client that is not registered with the server
// yet. Register the client with the server. Make a log upload request. Have
// serialization and log upload succeed. Verify that the delegate is notified of
// the success.
TEST_F(AppInstallEventLogUploaderTest, RegisterRequestSerializeAndUpload) {
  CreateUploader();
  RegisterClient();

  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  uploader_->RequestUpload();
}

// Create the uploader using a client that is not registered with the server
// yet. Make a log upload request. Verify that serialization is not started.
// Then, register the client with the server. Verify that serialization is
// started. Have serialization and log upload succeed. Verify that the delegate
// is notified of the success.
TEST_F(AppInstallEventLogUploaderTest, RequestRegisterSerializeAndUpload) {
  CreateUploader();

  EXPECT_CALL(delegate_, SerializeForUpload_(_)).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  RegisterClient();
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Unregister the client from the server. Register the client with the server.
// Verify that a new serialization is started. Then, have serialization and log
// upload succeed. Verify that the delegate is notified of the success.
TEST_F(AppInstallEventLogUploaderTest,
       RequestSerializeUnregisterRegisterAndUpload) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::StatusCallback status_callback;
  CompleteSerializeAndCaptureUpload(&status_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  UnregisterClient();
  Mock::VerifyAndClearExpectations(&client_);

  log_.add_app_install_reports()->set_package(kPackageName);
  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  RegisterClient();
}

// Make a log upload request. Have serialization begin. Unregister the client
// from the server. Have serialization succeed. Verify that the serialization
// result is ignored and no upload is started. Then, register the client with
// the server. Verify that a new serialization is started. Then, have
// serialization and log upload succeed. Verify that the delegate is notified of
// the success.
TEST_F(AppInstallEventLogUploaderTest,
       RequestUnregisterSerializeRegisterAndUpload) {
  RegisterClient();
  CreateUploader();

  AppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  UnregisterClient();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, UploadRealtimeReport(_, _)).Times(0);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  std::move(serialization_callback).Run(&log_);
  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(&client_);

  log_.add_app_install_reports()->set_package(kPackageName);
  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  RegisterClient();
}

// Make a log upload request. Have serialization begin. Unregister the client
// from the server. Register the client with the server. Verify that a second
// serialization is requested. Then, have the first serialization succeed.
// Verify that he serialization result is ignored and no upload is started.
// Then, have the second serialization succeed. Verify that an upload is
// started. Then, have the upload succeed. Verify that the delegate is notified
// of the success.
TEST_F(AppInstallEventLogUploaderTest,
       RequestUnregisterRegisterSerializeAndUpload) {
  RegisterClient();
  CreateUploader();

  AppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback_1;
  CaptureSerialize(&serialization_callback_1);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  UnregisterClient();
  Mock::VerifyAndClearExpectations(&client_);

  AppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback_2;
  CaptureSerialize(&serialization_callback_2);
  RegisterClient();

  EXPECT_CALL(client_, UploadRealtimeReport(_, _)).Times(0);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  std::move(serialization_callback_1).Run(&log_);
  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(&client_);

  log_.add_app_install_reports()->set_package(kPackageName);
  CompleteUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  std::move(serialization_callback_2).Run(&log_);
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Remove the delegate. Verify that the upload is canceled in the client.
TEST_F(AppInstallEventLogUploaderTest, RequestAndRemoveDelegate) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::StatusCallback status_callback;
  CompleteSerializeAndCaptureUpload(&status_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  uploader_->SetDelegate(nullptr);
}

}  // namespace policy
