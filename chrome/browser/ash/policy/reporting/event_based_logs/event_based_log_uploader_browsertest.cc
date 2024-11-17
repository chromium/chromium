// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/event_based_logs/event_based_log_uploader.h"

#include <optional>

#include "base/files/file_util.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job_test_util.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

const ash::reporting::TriggerEventType kTriggerEventType =
    ash::reporting::TriggerEventType::OS_UPDATE_FAILED;

constexpr char kUploadId[] = "uploadId123";

std::string GetExpectedUploadParameters(const std::string& exported_filepath,
                                        const std::string& upload_id) {
  constexpr char kExpectedUploadParametersFormatter[] =
      R"({"File-Type":"event_based_log_file","Filename":"%s","Upload-ID":"%s","Uploaded-Event-Type":%d}
application/json)";
  return base::StringPrintf(kExpectedUploadParametersFormatter,
                            exported_filepath.c_str(), upload_id.c_str(),
                            kTriggerEventType);
}

class EventBasedLogUploaderBrowserTest : public MixinBasedInProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    // Reporting test environment needs to be created before the browser
    // creation is completed.
    reporting_test_storage_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();

    reporting_test_enviroment_ =
        ::reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            reporting_test_storage_);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();

    reporting_test_enviroment_.reset();
    reporting_test_storage_.reset();
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    // Set serial number for testing.
    statistics_provider_.SetMachineStatistic("serial_number", "000000");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
      target_dir_ = scoped_temp_dir_.GetPath();
    }

    EventBasedLogUploaderImpl::SetTargetDirForTesting(&target_dir_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    EventBasedLogUploaderImpl::SetTargetDirForTesting(nullptr);
    MixinBasedInProcessBrowserTest::TearDownInProcessBrowserTestFixture();
  }

  void VerifyFileNotEmpty(base::FilePath file) {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    std::optional<int64_t> file_size = base::GetFileSize(file);
    ASSERT_TRUE(file_size.has_value());
    EXPECT_GT(file_size.value(), 0);
  }

  scoped_refptr<reporting::test::TestStorageModule> reporting_storage() {
    return reporting_test_storage_;
  }
  const base::FilePath& target_dir() { return target_dir_; }

 private:
  scoped_refptr<reporting::test::TestStorageModule> reporting_test_storage_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  ash::system::FakeStatisticsProvider statistics_provider_;

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath target_dir_;

  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_, {}};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(EventBasedLogUploaderBrowserTest, SuccessWithUploadId) {
  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());

  EventBasedLogUploaderImpl log_uploader;

  base::test::TestFuture<reporting::Status> on_upload_completed_future;
  // Trigger log upload with an upload ID.
  log_uploader.UploadEventBasedLogs(
      /*data_collectors=*/{support_tool::DataCollectorType::CHROME_INTERNAL},
      kTriggerEventType,
      /*upload_id=*/kUploadId, on_upload_completed_future.GetCallback());

  ASSERT_EQ(on_upload_completed_future.Take(), reporting::Status::StatusOK());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  EXPECT_TRUE(event.mutable_upload_settings()->has_origin_path());
  base::FilePath exported_file(event.mutable_upload_settings()->origin_path());
  // Ensure that the resulting `exported_file` exists under target directory.
  EXPECT_EQ(exported_file.DirName(), target_dir());

  ash::reporting::TriggerEventDetails* trigger_event_details =
      event.mutable_trigger_event_details();
  ASSERT_TRUE(trigger_event_details);
  EXPECT_EQ(trigger_event_details->trigger_event_type(), kTriggerEventType);
  EXPECT_EQ(trigger_event_details->log_upload_id(), kUploadId);

  std::string expected_upload_parameters =
      GetExpectedUploadParameters(exported_file.BaseName().value(), kUploadId);
  EXPECT_EQ(expected_upload_parameters,
            *event.mutable_upload_settings()->mutable_upload_parameters());

  // Check contents of the resulting file.
  VerifyFileNotEmpty(exported_file);
}

IN_PROC_BROWSER_TEST_F(EventBasedLogUploaderBrowserTest,
                       SuccessWithoutUploadId) {
  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());

  EventBasedLogUploaderImpl log_uploader;

  base::test::TestFuture<reporting::Status> on_upload_completed_future;
  // Trigger log upload without an upload ID.
  log_uploader.UploadEventBasedLogs(
      /*data_collectors=*/{support_tool::DataCollectorType::CHROME_INTERNAL},
      kTriggerEventType,
      /*upload_id=*/std::nullopt, on_upload_completed_future.GetCallback());

  ASSERT_EQ(on_upload_completed_future.Take(), reporting::Status::StatusOK());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  EXPECT_TRUE(event.mutable_upload_settings()->has_origin_path());
  base::FilePath exported_file(event.mutable_upload_settings()->origin_path());
  // Ensure that the resulting `exported_file` exists under target directory.
  EXPECT_EQ(exported_file.DirName(), target_dir());

  ash::reporting::TriggerEventDetails* trigger_event_details =
      event.mutable_trigger_event_details();
  ASSERT_TRUE(trigger_event_details);
  EXPECT_EQ(trigger_event_details->trigger_event_type(), kTriggerEventType);
  // Verify that an upload ID is created for the event log.
  EXPECT_TRUE(trigger_event_details->has_log_upload_id());
  const std::string& upload_id = trigger_event_details->log_upload_id();
  EXPECT_FALSE(upload_id.empty());

  std::string expected_upload_parameters =
      GetExpectedUploadParameters(exported_file.BaseName().value(), upload_id);
  EXPECT_EQ(expected_upload_parameters,
            *event.mutable_upload_settings()->mutable_upload_parameters());

  // Check contents of the resulting file.
  VerifyFileNotEmpty(exported_file);
}

IN_PROC_BROWSER_TEST_F(EventBasedLogUploaderBrowserTest,
                       FailWhenOngoingUploadAlreadyExists) {
  EventBasedLogUploaderImpl log_uploader;

  base::test::TestFuture<reporting::Status> first_upload_future;
  // Trigger first log upload.
  log_uploader.UploadEventBasedLogs(
      /*data_collectors=*/{support_tool::DataCollectorType::CHROME_INTERNAL},
      kTriggerEventType,
      /*upload_id=*/std::nullopt, first_upload_future.GetCallback());

  base::test::TestFuture<reporting::Status> second_upload_future;
  // Trigger second log upload. This should fail because there's already ongoing
  // first upload.
  log_uploader.UploadEventBasedLogs(
      /*data_collectors=*/{support_tool::DataCollectorType::CHROME_INTERNAL},
      kTriggerEventType,
      /*upload_id=*/std::nullopt, second_upload_future.GetCallback());
  reporting::Status upload_status = second_upload_future.Take();
  ASSERT_NE(upload_status, reporting::Status::StatusOK());
  ASSERT_EQ(upload_status.error_code(), reporting::error::Code::ALREADY_EXISTS);
}

}  // namespace policy
