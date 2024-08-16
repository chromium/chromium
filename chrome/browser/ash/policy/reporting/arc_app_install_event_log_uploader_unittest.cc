// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log_uploader.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/reporting/install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::Mock;
using testing::WithArgs;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::TimeDelta kMinRetryBackoff = base::Seconds(10);
constexpr base::TimeDelta kMaxRetryBackoff = base::Days(1);

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

class MockArcAppInstallEventLogUploaderDelegate
    : public ArcAppInstallEventLogUploader::Delegate {
 public:
  MockArcAppInstallEventLogUploaderDelegate() {}

  MockArcAppInstallEventLogUploaderDelegate(
      const MockArcAppInstallEventLogUploaderDelegate&) = delete;
  MockArcAppInstallEventLogUploaderDelegate& operator=(
      const MockArcAppInstallEventLogUploaderDelegate&) = delete;

  void SerializeForUpload(SerializationCallback callback) override {
    SerializeForUpload_(callback);
  }

  MOCK_METHOD1(SerializeForUpload_, void(SerializationCallback&));
  MOCK_METHOD0(OnUploadSuccess, void());
};

}  // namespace

class ArcAppInstallEventLogUploaderTest : public testing::Test {
 public:
  ArcAppInstallEventLogUploaderTest(const ArcAppInstallEventLogUploaderTest&) =
      delete;
  ArcAppInstallEventLogUploaderTest& operator=(
      const ArcAppInstallEventLogUploaderTest&) = delete;

 protected:
  ArcAppInstallEventLogUploaderTest() = default;

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
    uploader_ = std::make_unique<ArcAppInstallEventLogUploader>(
        &client_, /*profile=*/nullptr);
    uploader_->SetDelegate(&delegate_);
  }

  void CompleteSerialize() {
    EXPECT_CALL(delegate_, SerializeForUpload_)
        .WillOnce(WithArgs<0>(Invoke(
            [=, this](
                ArcAppInstallEventLogUploader::Delegate::SerializationCallback&
                    callback) { std::move(callback).Run(&log_); })));
  }

  void CaptureSerialize(
      ArcAppInstallEventLogUploader::Delegate::SerializationCallback*
          callback) {
    EXPECT_CALL(delegate_, SerializeForUpload_).WillOnce(MoveArg<0>(callback));
  }

  void CompleteUpload(bool success) {
    value_report_.clear();
    base::Value::Dict context = reporting::GetContext(/*profile=*/nullptr);
    base::Value::List events = ConvertArcAppProtoToValue(&log_, context);
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(events), std::move(context));

    EXPECT_CALL(client_, UploadAppInstallReport(MatchValue(&value_report_), _))
        .WillOnce(
            WithArgs<1>(Invoke([=](CloudPolicyClient::ResultCallback callback) {
              std::move(callback).Run(CloudPolicyClient::Result(
                  success ? DM_STATUS_SUCCESS
                          : DM_STATUS_TEMPORARY_UNAVAILABLE));
            })));
  }

  void CaptureUpload(CloudPolicyClient::ResultCallback* callback) {
    value_report_.clear();
    base::Value::Dict context = reporting::GetContext(/*profile=*/nullptr);
    base::Value::List events = ConvertArcAppProtoToValue(&log_, context);
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(events), std::move(context));

    CloudPolicyClient::StatusCallback status_callback;
    EXPECT_CALL(client_, UploadAppInstallReport(MatchValue(&value_report_), _))
        .WillOnce(MoveArg<1>(callback));
  }

  void CompleteSerializeAndUpload(bool success) {
    CompleteSerialize();
    CompleteUpload(success);
  }

  void CompleteSerializeAndCaptureUpload(
      CloudPolicyClient::ResultCallback* callback) {
    CompleteSerialize();
    CaptureUpload(callback);
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  em::AppInstallReportRequest log_;
  base::Value::Dict value_report_;

  MockCloudPolicyClient client_;
  MockArcAppInstallEventLogUploaderDelegate delegate_;
  std::unique_ptr<ArcAppInstallEventLogUploader> uploader_;

  ash::system::ScopedFakeStatisticsProvider scoped_fake_statistics_provider_;
};

// Make a log upload request. Have serialization and log upload succeed. Verify
// that the delegate is notified of the success.
TEST_F(ArcAppInstallEventLogUploaderTest, RequestSerializeAndUpload) {
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
TEST_F(ArcAppInstallEventLogUploaderTest, RequestSerializeRequestAndUpload) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::ResultCallback status_callback;
  CompleteSerializeAndCaptureUpload(&status_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeForUpload_).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, OnUploadSuccess());
  EXPECT_CALL(delegate_, SerializeForUpload_).Times(0);
  std::move(status_callback).Run(CloudPolicyClient::Result(DM_STATUS_SUCCESS));
}

// Make a log upload request. Have serialization begin. Make a second upload
// request. Verify that no serialization is started for the second request.
// Then, have the first request's serialization and upload succeed. Verify that
// the delegate is notified of the first request's success.
TEST_F(ArcAppInstallEventLogUploaderTest, RequestRequestSerializeAndUpload) {
  RegisterClient();
  CreateUploader();

  ArcAppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeForUpload_).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  CompleteUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization begin. Cancel the request. Have
// the serialization succeed. Verify that the serialization result is ignored
// and no upload is started.
TEST_F(ArcAppInstallEventLogUploaderTest, RequestCancelAndSerialize) {
  RegisterClient();
  CreateUploader();

  ArcAppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  uploader_->CancelUpload();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, UploadAppInstallReport).Times(0);
  EXPECT_CALL(delegate_, OnUploadSuccess()).Times(0);
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Cancel the request. Verify that the upload is canceled in the client.
TEST_F(ArcAppInstallEventLogUploaderTest, RequestSerializeAndCancel) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::ResultCallback result_callback;
  CompleteSerializeAndCaptureUpload(&result_callback);
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
TEST_F(ArcAppInstallEventLogUploaderTest, Retry) {
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
TEST_F(ArcAppInstallEventLogUploaderTest, RegisterRequestSerializeAndUpload) {
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
TEST_F(ArcAppInstallEventLogUploaderTest, RequestRegisterSerializeAndUpload) {
  CreateUploader();

  EXPECT_CALL(delegate_, SerializeForUpload_).Times(0);
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
TEST_F(ArcAppInstallEventLogUploaderTest,
       RequestSerializeUnregisterRegisterAndUpload) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::ResultCallback result_callback;
  CompleteSerializeAndCaptureUpload(&result_callback);
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
TEST_F(ArcAppInstallEventLogUploaderTest,
       RequestUnregisterSerializeRegisterAndUpload) {
  RegisterClient();
  CreateUploader();

  ArcAppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  UnregisterClient();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, UploadAppInstallReport).Times(0);
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
TEST_F(ArcAppInstallEventLogUploaderTest,
       RequestUnregisterRegisterSerializeAndUpload) {
  RegisterClient();
  CreateUploader();

  ArcAppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback_1;
  CaptureSerialize(&serialization_callback_1);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  UnregisterClient();
  Mock::VerifyAndClearExpectations(&client_);

  ArcAppInstallEventLogUploader::Delegate::SerializationCallback
      serialization_callback_2;
  CaptureSerialize(&serialization_callback_2);
  RegisterClient();

  EXPECT_CALL(client_, UploadAppInstallReport).Times(0);
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
TEST_F(ArcAppInstallEventLogUploaderTest, RequestAndRemoveDelegate) {
  RegisterClient();
  CreateUploader();

  CloudPolicyClient::ResultCallback result_callback;
  CompleteSerializeAndCaptureUpload(&result_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&client_);

  EXPECT_CALL(client_, CancelAppInstallReportUpload());
  uploader_->SetDelegate(nullptr);
}

// When there is more than one identical event in the log, ensure that only one
// of those duplicate events is in the created report.
TEST_F(ArcAppInstallEventLogUploaderTest, DuplicateEvents) {
  RegisterClient();
  CreateUploader();

  em::AppInstallReport* report = log_.add_app_install_reports();
  report->set_package(kPackageName);

  // Adding 3 events, but the first two are identical, so the final report
  // should only contain 2 events.
  em::AppInstallReportLogEvent* ev1 = report->add_logs();
  ev1->set_event_type(em::AppInstallReportLogEvent::SUCCESS);
  ev1->set_timestamp(0);

  em::AppInstallReportLogEvent* ev2 = report->add_logs();
  ev2->set_event_type(em::AppInstallReportLogEvent::SUCCESS);
  ev2->set_timestamp(0);

  em::AppInstallReportLogEvent* ev3 = report->add_logs();
  ev3->set_event_type(em::AppInstallReportLogEvent::SUCCESS);
  ev3->set_timestamp(1000);

  CompleteSerializeAndUpload(true /* success */);
  EXPECT_CALL(delegate_, OnUploadSuccess());
  uploader_->RequestUpload();

  EXPECT_EQ(2u, value_report_
                    .FindList(RealtimeReportingJobConfiguration::kEventListKey)
                    ->size());
}

}  // namespace policy
