// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/extension_install_event_log_uploader.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/test/gmock_move_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/install_event_log_util.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/mock_cloud_policy_client.h"
#include "components/policy/core/common/cloud/realtime_reporting_job_configuration.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Mock;
using testing::WithArgs;

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr base::TimeDelta kMinRetryBackoff = base::TimeDelta::FromSeconds(10);
constexpr base::TimeDelta kMaxRetryBackoff = base::TimeDelta::FromDays(1);

static const char kExtensionId[] = "abcdefghabcdefghabcdefghabcdefgh";

MATCHER_P(MatchEvents, expected, "contains events") {
  DCHECK(expected);
  std::string expected_serialized_string;
  JSONStringValueSerializer expected_serializer(&expected_serialized_string);
  if (!expected_serializer.Serialize(*expected))
    return false;

  return arg == expected_serialized_string;
}

class MockExtensionInstallEventLogUploaderDelegate
    : public ExtensionInstallEventLogUploader::Delegate {
 public:
  MockExtensionInstallEventLogUploaderDelegate() {}

  void SerializeExtensionLogForUpload(
      ExtensionLogSerializationCallback callback) override {
    SerializeExtensionLogForUpload_(callback);
  }

  MOCK_METHOD1(SerializeExtensionLogForUpload_,
               void(ExtensionLogSerializationCallback&));
  MOCK_METHOD0(OnExtensionLogUploadSuccess, void());
};

}  // namespace

class ExtensionInstallEventLogUploaderTest : public testing::Test {
 protected:
  ExtensionInstallEventLogUploaderTest() = default;

  void SetUp() override {
    CreateUploader();
    waiter_ = std::make_unique<reporting::test::TestCallbackWaiter>();
  }

  void TearDown() override {
    waiter_->Wait();
    Mock::VerifyAndClearExpectations(mock_report_queue_);
    Mock::VerifyAndClearExpectations(&delegate_);
    uploader_.reset();
  }

  void WaitAndReset() {
    waiter_->Wait();
    waiter_ = std::make_unique<reporting::test::TestCallbackWaiter>();
  }

  void CreateUploader() {
    uploader_ = std::make_unique<ExtensionInstallEventLogUploader>(
        /*profile=*/nullptr);
    uploader_->SetDelegate(&delegate_);

    auto mock_report_queue = std::make_unique<reporting::MockReportQueue>();
    mock_report_queue_ = mock_report_queue.get();
    uploader_->SetReportQueue(std::move(mock_report_queue));
  }

  void CompleteSerialize() {
    waiter_->Attach();
    EXPECT_CALL(delegate_, SerializeExtensionLogForUpload_(_))
        .WillOnce(WithArgs<0>(
            Invoke([=](ExtensionInstallEventLogUploader::Delegate::
                           ExtensionLogSerializationCallback& callback) {
              std::move(callback).Run(&log_);
              waiter_->Signal();
            })));
  }

  void CaptureSerialize(ExtensionInstallEventLogUploader::Delegate::
                            ExtensionLogSerializationCallback* callback) {
    waiter_->Attach();
    EXPECT_CALL(delegate_, SerializeExtensionLogForUpload_(_))
        .WillOnce(
            DoAll(MoveArg<0>(callback), Invoke([=]() { waiter_->Signal(); })));
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
    base::Value context = reporting::GetContext(/*profile=*/nullptr);
    base::Value events = ConvertExtensionProtoToValue(&log_, context);
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(events), std::move(context));

    waiter_->Attach();

    EXPECT_CALL(*mock_report_queue_,
                AddRecord(MatchEvents(&value_report_), _, _))
        .WillOnce(
            Invoke([=](base::StringPiece, reporting::Priority priority,
                       reporting::MockReportQueue::EnqueueCallback callback) {
              reporting::Status status =
                  success ? reporting::Status::StatusOK()
                          : reporting::Status(reporting::error::INTERNAL,
                                              "Failing for tests");
              std::move(callback).Run(status);
              waiter_->Signal();

              // In the real ReportEnqueue::ValueEnqueue call this status return
              // would indicate the that storage module is unavailable. From
              // ExtensionInstallEventLogUploader, it follows the same execution
              // path of failing UploadDone.
              return reporting::Status::StatusOK();
            }));
  }

  void CaptureUpload(reporting::MockReportQueue::EnqueueCallback* callback) {
    ClearReportDict();
    base::Value context = reporting::GetContext(/*profile=*/nullptr);
    base::Value events = ConvertExtensionProtoToValue(&log_, context);
    value_report_ = RealtimeReportingJobConfiguration::BuildReport(
        std::move(events), std::move(context));

    EXPECT_CALL(*mock_report_queue_,
                AddRecord(MatchEvents(&value_report_), _, _))
        .WillOnce(
            Invoke([callback](base::StringPiece, reporting::Priority priority,
                              reporting::MockReportQueue::EnqueueCallback cb) {
              *callback = std::move(cb);
              return reporting::Status::StatusOK();
            }));
  }

  void CompleteSerializeAndUpload(bool success) {
    CompleteSerialize();
    CompleteUpload(success);
  }

  void CompleteSerializeAndCaptureUpload(
      reporting::MockReportQueue::EnqueueCallback* callback) {
    CompleteSerialize();
    CaptureUpload(callback);
  }

  void ExpectExtensionLogUploadSuccess() {
    waiter_->Attach();
    EXPECT_CALL(delegate_, OnExtensionLogUploadSuccess())
        .WillOnce(Invoke([=]() { waiter_->Signal(); }));
  }

  // Setup retry by serializing event, but failing to upload.
  void SetupForRetry() {
    CompleteSerializeAndUpload(false /* success */);
    EXPECT_CALL(delegate_, OnExtensionLogUploadSuccess()).Times(0);
    uploader_->RequestUpload();

    WaitAndReset();

    Mock::VerifyAndClearExpectations(&delegate_);
    Mock::VerifyAndClearExpectations(mock_report_queue_);

    // A task is enqueued with zero delay and needs to be processed.
    base::TimeDelta zero_delay = base::TimeDelta::FromSeconds(0);

    // Expect and throwaway task.
    EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(), zero_delay);
    task_environment_.FastForwardBy(zero_delay);
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  em::ExtensionInstallReportRequest log_;
  base::Value value_report_{base::Value::Type::DICTIONARY};

  reporting::MockReportQueue* mock_report_queue_;
  MockExtensionInstallEventLogUploaderDelegate delegate_;
  std::unique_ptr<ExtensionInstallEventLogUploader> uploader_;

  chromeos::system::ScopedFakeStatisticsProvider
      scoped_fake_statistics_provider_;
  std::unique_ptr<reporting::test::TestCallbackWaiter> waiter_;
};

// Make a log upload request. Have serialization and log upload succeed. Verify
// that the delegate is notified of the success.
TEST_F(ExtensionInstallEventLogUploaderTest, RequestSerializeAndUpload) {
  CompleteSerializeAndUpload(true /* success */);
  ExpectExtensionLogUploadSuccess();
  uploader_->RequestUpload();
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Make a second upload request. Have the first upload succeed. Verify that the
// delegate is notified of the first request's success and no serialization is
// started for the second request.
TEST_F(ExtensionInstallEventLogUploaderTest, RequestSerializeRequestAndUpload) {
  reporting::MockReportQueue::EnqueueCallback upload_callback;
  CompleteSerializeAndCaptureUpload(&upload_callback);
  uploader_->RequestUpload();

  WaitAndReset();

  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeExtensionLogForUpload_(_)).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  ExpectExtensionLogUploadSuccess();
  EXPECT_CALL(delegate_, SerializeExtensionLogForUpload_(_)).Times(0);
  std::move(upload_callback).Run(reporting::Status::StatusOK());
}

// Make a log upload request. Have serialization begin. Make a second upload
// request. Verify that no serialization is started for the second request.
// Then, have the first request's serialization and upload succeed. Verify that
// the delegate is notified of the first request's success.
TEST_F(ExtensionInstallEventLogUploaderTest, RequestRequestSerializeAndUpload) {
  ExtensionInstallEventLogUploader::Delegate::ExtensionLogSerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();

  WaitAndReset();

  Mock::VerifyAndClearExpectations(&delegate_);

  EXPECT_CALL(delegate_, SerializeExtensionLogForUpload_(_)).Times(0);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(&delegate_);

  CompleteUpload(true /* success */);
  ExpectExtensionLogUploadSuccess();
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization begin. Cancel the request. Have
// the serialization succeed. Verify that the serialization result is ignored
// and no upload is started.
TEST_F(ExtensionInstallEventLogUploaderTest, RequestCancelAndSerialize) {
  ExtensionInstallEventLogUploader::Delegate::ExtensionLogSerializationCallback
      serialization_callback;
  CaptureSerialize(&serialization_callback);
  uploader_->RequestUpload();

  WaitAndReset();

  Mock::VerifyAndClearExpectations(&delegate_);

  uploader_->CancelUpload();
  Mock::VerifyAndClearExpectations(mock_report_queue_);

  EXPECT_CALL(*mock_report_queue_, AddRecord(_, _, _)).Times(0);
  EXPECT_CALL(delegate_, OnExtensionLogUploadSuccess()).Times(0);
  std::move(serialization_callback).Run(&log_);
}

// Make a log upload request. Have serialization succeed and log upload begin.
// Cancel the request.
TEST_F(ExtensionInstallEventLogUploaderTest, RequestSerializeAndCancel) {
  reporting::MockReportQueue::EnqueueCallback upload_callback;
  CompleteSerializeAndCaptureUpload(&upload_callback);
  uploader_->RequestUpload();
  Mock::VerifyAndClearExpectations(mock_report_queue_);

  uploader_->CancelUpload();
}

// Make a log upload request. Have serialization succeed but log upload fail.
// Verify that serialization and log upload are retried with exponential
// backoff. Have the retries fail until the maximum backoff is seen twice. Then,
// have serialization and log upload succeed. Verify that the delegate is
// notified of the success. Then, make another log upload request. Have the
// serialization succeed but log upload fail again. Verify that the backoff has
// returned to the minimum.
TEST_F(ExtensionInstallEventLogUploaderTest, Retry) {
  SetupForRetry();

  const base::TimeDelta min_delay = kMinRetryBackoff;
  const base::TimeDelta max_delay = kMaxRetryBackoff;

  base::TimeDelta expected_delay = min_delay;
  int max_delay_count = 0;
  while (max_delay_count < 2) {
    // Make sure next upload attempt is scheduled correctly.
    EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(),
              expected_delay);

    // Setup expectations for upload attempt.
    CompleteSerializeAndUpload(false /* success */);
    EXPECT_CALL(delegate_, OnExtensionLogUploadSuccess()).Times(0);

    // FastForward until upload attempts are complete.
    task_environment_.FastForwardBy(expected_delay);

    WaitAndReset();

    if (expected_delay == max_delay) {
      ++max_delay_count;
    }

    expected_delay = std::min(expected_delay * 2, max_delay);
  }
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(), expected_delay);

  // Allow Upload to succeed.
  log_.add_extension_install_reports()->set_extension_id(kExtensionId);
  CompleteSerializeAndUpload(true /* success */);
  ExpectExtensionLogUploadSuccess();

  task_environment_.FastForwardBy(expected_delay);

  WaitAndReset();

  Mock::VerifyAndClearExpectations(&delegate_);
  Mock::VerifyAndClearExpectations(mock_report_queue_);

  // Ensure upload fails and retry delay happens again.
  SetupForRetry();
  EXPECT_EQ(task_environment_.NextMainThreadPendingTaskDelay(), min_delay);
}

// When there is more than one identical event in the log, ensure that only one
// of those duplicate events is in the created report.
TEST_F(ExtensionInstallEventLogUploaderTest, DuplicateEvents) {
  em::ExtensionInstallReport* report = log_.add_extension_install_reports();
  report->set_extension_id(kExtensionId);

  // Adding 3 events, but the first two are identical, so the final report
  // should only contain 2 events.
  em::ExtensionInstallReportLogEvent* ev1 = report->add_logs();
  ev1->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  ev1->set_timestamp(0);

  em::ExtensionInstallReportLogEvent* ev2 = report->add_logs();
  ev2->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  ev2->set_timestamp(0);

  em::ExtensionInstallReportLogEvent* ev3 = report->add_logs();
  ev3->set_event_type(em::ExtensionInstallReportLogEvent::SUCCESS);
  ev3->set_timestamp(1000);

  CompleteSerializeAndUpload(true /* success */);
  ExpectExtensionLogUploadSuccess();
  uploader_->RequestUpload();

  WaitAndReset();
  EXPECT_EQ(2u,
            value_report_
                .FindListKey(RealtimeReportingJobConfiguration::kEventListKey)
                ->GetList()
                .size());
}

}  // namespace policy
