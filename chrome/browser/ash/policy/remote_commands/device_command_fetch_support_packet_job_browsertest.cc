// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job_test_util.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome_device_policy.pb.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/storage/test_storage_module.h"
#include "content/public/browser/browser_main_parts.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

using base::test::IsJson;
using ::testing::_;
using ::testing::WithArg;

namespace policy {

namespace {

// Use a number larger than int32 to catch truncation errors.
const int64_t kInitialCommandId = (1LL << 35) + 1;

class DeviceCommandFetchSupportPacketBrowserTest
    : public DevicePolicyCrosBrowserTest {
 protected:
  void CreatedBrowserMainParts(
      content::BrowserMainParts* browser_main_parts) override {
    DevicePolicyCrosBrowserTest::CreatedBrowserMainParts(browser_main_parts);
    // Reporting test environment needs to be created before the browser
    // creation is completed.
    reporting_test_storage_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();

    reporting_test_enviroment_ =
        reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            reporting_test_storage_);
  }

  void SetUpInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();

    remote_commands_service_mixin_.SetCurrentIdForTesting(kInitialCommandId);

    // Set serial number for testing.
    statistics_provider_.SetMachineStatistic("serial_number", "000000");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
      target_dir_ = scoped_temp_dir_.GetPath();
    }

    DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(&target_dir_);
  }

  void TearDownInProcessBrowserTestFixture() override {
    DevicePolicyCrosBrowserTest::TearDownInProcessBrowserTestFixture();
    DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(nullptr);
  }

  int64_t WaitForCommandExecution(
      const enterprise_management::RemoteCommand& command) {
    int64_t command_id =
        remote_commands_service_mixin_.AddPendingRemoteCommand(command);
    remote_commands_service_mixin_.SendDeviceRemoteCommandsRequest();
    remote_commands_service_mixin_.WaitForAcked(command_id);
    return command_id;
  }

  enterprise_management::RemoteCommandResult WaitForCommandResult(
      const enterprise_management::RemoteCommand& command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

  void SetLogUploadEnabledPolicy(bool enabled) {
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    proto.mutable_device_log_upload_settings()->set_system_log_upload_enabled(
        enabled);
    policy_helper()->RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kSystemLogUploadEnabled});
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }
  scoped_refptr<reporting::test::TestStorageModule> reporting_storage() {
    return reporting_test_storage_;
  }
  const base::FilePath& target_dir() { return target_dir_; }

 private:
  scoped_refptr<reporting::test::TestStorageModule> reporting_test_storage_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  ash::system::FakeStatisticsProvider statistics_provider_;
  base::HistogramTester histogram_tester_;

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath target_dir_;

  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_, {}};
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  RemoteCommandsServiceMixin remote_commands_service_mixin_{
      mixin_host_, policy_test_server_mixin_};
};

}  // namespace

// TODO: b/313072234 - Add tests for different session types. For now, we only
// test on login screen (without any session).
IN_PROC_BROWSER_TEST_F(DeviceCommandFetchSupportPacketBrowserTest, Success) {
  SetLogUploadEnabledPolicy(true);
  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());
  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      GetAllAvailableDataCollectorsOnDevice()));
  ASSERT_TRUE(payload.has_value());
  int64_t command_id = WaitForCommandExecution(
      RemoteCommandBuilder()
          .SetType(em::RemoteCommand::FETCH_SUPPORT_PACKET)
          .SetPayload(payload.value())
          .Build());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  EXPECT_TRUE(event.mutable_upload_settings()->has_origin_path());
  base::FilePath exported_file(event.mutable_upload_settings()->origin_path());
  // Ensure that the resulting `exported_file` exist under target directory.
  EXPECT_EQ(exported_file.DirName(), target_dir());
  EXPECT_TRUE(event.has_command_id());
  EXPECT_EQ(event.command_id(), command_id);

  std::string expected_upload_parameters = test::GetExpectedUploadParameters(
      command_id, exported_file.BaseName().value());
  EXPECT_EQ(expected_upload_parameters,
            *event.mutable_upload_settings()->mutable_upload_parameters());

  // The result payload should contain the success result code.
  base::Value::Dict expected_payload;
  expected_payload.Set("result",
                       enterprise_management::FetchSupportPacketResultCode::
                           FETCH_SUPPORT_PACKET_RESULT_SUCCESS);
  EXPECT_THAT(event.command_result_payload(),
              IsJson(std::move(expected_payload)));

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_F(DeviceCommandFetchSupportPacketBrowserTest,
                       FailWhenLogUploadDisabled) {
  SetLogUploadEnabledPolicy(false);
  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      GetAllAvailableDataCollectorsOnDevice()));
  ASSERT_TRUE(payload.has_value());
  enterprise_management::RemoteCommandResult result =
      WaitForCommandResult(RemoteCommandBuilder()
                               .SetType(em::RemoteCommand::FETCH_SUPPORT_PACKET)
                               .SetPayload(payload.value())
                               .Build());
  EXPECT_EQ(result.result(),
            enterprise_management::RemoteCommandResult_ResultType::
                RemoteCommandResult_ResultType_RESULT_FAILURE);
  // Expect result payload when the command fails because of not being
  // supported on the device.
  EXPECT_THAT(
      result.payload(),
      IsJson(base::Value::Dict().Set(
          "result", enterprise_management::FetchSupportPacketResultCode::
                        FAILURE_COMMAND_NOT_ENABLED)));

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::
          kFailedOnCommandEnabledForUserCheck,
      1);
}

}  // namespace policy
