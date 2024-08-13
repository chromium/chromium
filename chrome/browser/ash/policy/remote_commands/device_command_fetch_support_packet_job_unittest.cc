// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job.h"

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"
#include "chrome/browser/ash/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job_test_util.h"
#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/debug_daemon/debug_daemon_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/feedback/redaction_tool/pii_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/storage/storage_module_interface.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/reporting/util/status.h"
#include "components/user_manager/scoped_user_manager.h"
#include "record.pb.h"
#include "record_constants.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::IsJson;
using ::testing::_;
using ::testing::WithArg;

namespace policy {

using test::TestSessionType;

namespace em = enterprise_management;

namespace {

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456;
// The age of command in milliseconds.
constexpr int64 kCommandAge = 60000;

em::RemoteCommand GenerateCommandProto(std::string payload) {
  em::RemoteCommand command_proto;
  command_proto.set_type(em::RemoteCommand_Type_FETCH_SUPPORT_PACKET);
  command_proto.set_command_id(kUniqueID);
  command_proto.set_age_of_command(kCommandAge);
  command_proto.set_payload(payload);
  return command_proto;
}

class DeviceCommandFetchSupportPacketTest : public ash::DeviceSettingsTestBase {
 public:
  DeviceCommandFetchSupportPacketTest()
      : ash::DeviceSettingsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  DeviceCommandFetchSupportPacketTest(
      const DeviceCommandFetchSupportPacketTest&) = delete;
  DeviceCommandFetchSupportPacketTest& operator=(
      const DeviceCommandFetchSupportPacketTest&) = delete;

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());

    reporting_test_storage_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();
    reporting_test_enviroment_ =
        reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            reporting_test_storage_);

    ash::DebugDaemonClient::InitializeFake();
    // Set serial number for testing.
    statistics_provider_.SetMachineStatistic("serial_number", "000000");
    ash::system::StatisticsProvider::SetTestProvider(&statistics_provider_);
    cros_settings_helper_.ReplaceDeviceSettingsProviderWithStub();

    web_kiosk_app_manager_ = std::make_unique<ash::WebKioskAppManager>();
    kiosk_chrome_app_manager_ = std::make_unique<ash::KioskChromeAppManager>();

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
      target_dir_ = temp_dir_.GetPath();
    }

    DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(&target_dir_);
  }

  void TearDown() override {
    DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(nullptr);

    kiosk_chrome_app_manager_.reset();
    web_kiosk_app_manager_.reset();

    ash::DebugDaemonClient::Shutdown();
    DeviceSettingsTestBase::TearDown();
  }

  void SetLogUploadEnabledPolicy(bool enabled) {
    cros_settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, enabled);
  }

  void StartSessionOfType(TestSessionType session_type) {
    // `user_manager_` is inherited from ash::DeviceSettingsTestBase.
    StartSessionOfTypeWithProfile(session_type, *user_manager_,
                                  profile_manager_);
  }

  void InitAndRunCommandJob(DeviceCommandFetchSupportPacketJob& in_job,
                            const em::RemoteCommand& command) {
    EXPECT_TRUE(in_job.Init(base::TimeTicks::Now(), command, em::SignedData()));

    base::test::TestFuture<void> job_finished_future;
    bool success = in_job.Run(base::Time::Now(), base::TimeTicks::Now(),
                              job_finished_future.GetCallback());
    EXPECT_TRUE(success);
    ASSERT_TRUE(job_finished_future.Wait()) << "Job did not finish.";
  }

 protected:
  // App manager instances for testing kiosk sessions.
  std::unique_ptr<ash::WebKioskAppManager> web_kiosk_app_manager_;
  std::unique_ptr<ash::KioskChromeAppManager> kiosk_chrome_app_manager_;

  scoped_refptr<reporting::test::TestStorageModule> reporting_test_storage_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  ash::system::FakeStatisticsProvider statistics_provider_;
  ash::ScopedCrosSettingsTestHelper cros_settings_helper_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      user_manager_{std::make_unique<ash::FakeChromeUserManager>()};
  base::HistogramTester histogram_tester_;
  TestingProfileManager profile_manager_{TestingBrowserProcess::GetGlobal()};
  base::FilePath target_dir_;
  base::ScopedTempDir temp_dir_;
};

// Fixture for tests parameterized over the possible user session
// types(`TestSessionType`) and PII policies.
class DeviceCommandFetchSupportPacketTestParameterized
    : public DeviceCommandFetchSupportPacketTest,
      public ::testing::WithParamInterface<test::SessionInfo> {};

}  // namespace

TEST_F(DeviceCommandFetchSupportPacketTest,
       FailIfPayloadContainsEmptyDataCollectors) {
  DeviceCommandFetchSupportPacketJob job;
  // Wrong payload with empty data collectors list.
  base::Value::Dict command_payload =
      test::GetFetchSupportPacketCommandPayloadDict(
          {support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS});
  command_payload.SetByDottedPath(
      "supportPacketDetails.requestedDataCollectors", base::Value::List());
  auto wrong_payload = base::WriteJson(std::move(command_payload));
  ASSERT_TRUE(wrong_payload.has_value());

  // Shouldn't be able to initialize with wrong payload.
  EXPECT_FALSE(job.Init(base::TimeTicks::Now(),
                        GenerateCommandProto(wrong_payload.value()),
                        em::SignedData()));
  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kFailedOnWrongCommandPayload, 1);
}

TEST_F(DeviceCommandFetchSupportPacketTest, FailWhenLogUploadDisabled) {
  StartSessionOfType(TestSessionType::kNoSession);
  SetLogUploadEnabledPolicy(/*enabled=*/false);

  DeviceCommandFetchSupportPacketJob job;

  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      {support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS}));
  ASSERT_TRUE(payload.has_value());

  InitAndRunCommandJob(job, GenerateCommandProto(payload.value()));

  EXPECT_EQ(job.status(), RemoteCommandJob::FAILED);
  // Expect result payload when the command fails because of not being
  // supported on the device.
  EXPECT_THAT(
      *job.GetResultPayload(),
      IsJson(base::Value::Dict().Set(
          "result", enterprise_management::FetchSupportPacketResultCode::
                        FAILURE_COMMAND_NOT_ENABLED)));

  histogram_tester_.ExpectUniqueSample(kFetchSupportPacketFailureHistogramName,
                                       EnterpriseFetchSupportPacketFailureType::
                                           kFailedOnCommandEnabledForUserCheck,
                                       1);
}

TEST_P(DeviceCommandFetchSupportPacketTestParameterized,
       SuccessfulCommandRequestWithoutPii) {
  const test::SessionInfo& session_info = GetParam();
  StartSessionOfType(session_info.session_type);
  SetLogUploadEnabledPolicy(/*enabled=*/true);

  DeviceCommandFetchSupportPacketJob job;

  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_test_storage_, log_upload_event_future.GetRepeatingCallback());

  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      {support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS}));
  ASSERT_TRUE(payload.has_value());
  InitAndRunCommandJob(job, GenerateCommandProto(payload.value()));

  EXPECT_EQ(job.status(), RemoteCommandJob::ACKED);

  ash::reporting::LogUploadEvent enqueued_event =
      log_upload_event_future.Take();
  EXPECT_TRUE(enqueued_event.mutable_upload_settings()->has_origin_path());
  base::FilePath exported_file(
      enqueued_event.mutable_upload_settings()->origin_path());
  // Ensure that the resulting `exported_file` exist under target directory.
  EXPECT_EQ(exported_file.DirName(), target_dir_);
  // Check the contents of LogUploadEvent that the job enqueued.
  std::string expected_upload_parameters = test::GetExpectedUploadParameters(
      kUniqueID, exported_file.BaseName().value());
  EXPECT_EQ(
      expected_upload_parameters,
      *enqueued_event.mutable_upload_settings()->mutable_upload_parameters());
  EXPECT_TRUE(enqueued_event.has_remote_command_details());
  // The result payload should contain the success result code.
  EXPECT_THAT(
      enqueued_event.remote_command_details().command_result_payload(),
      IsJson(base::Value::Dict().Set(
          "result", enterprise_management::FetchSupportPacketResultCode::
                        FETCH_SUPPORT_PACKET_RESULT_SUCCESS)));
  EXPECT_EQ(enqueued_event.remote_command_details().command_id(), kUniqueID);

  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

TEST_P(DeviceCommandFetchSupportPacketTestParameterized,
       SuccessfulCommandRequestWithPii) {
  const test::SessionInfo& session_info = GetParam();
  StartSessionOfType(session_info.session_type);
  SetLogUploadEnabledPolicy(/*enabled=*/true);

  DeviceCommandFetchSupportPacketJob job;

  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_test_storage_, log_upload_event_future.GetRepeatingCallback());

  // Add a requested PII type to the command payload.
  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      {support_tool::DataCollectorType::CHROMEOS_SYSTEM_LOGS},
      {support_tool::PiiType::EMAIL}));
  ASSERT_TRUE(payload.has_value());
  InitAndRunCommandJob(job, GenerateCommandProto(payload.value()));

  EXPECT_EQ(job.status(), RemoteCommandJob::ACKED);

  ash::reporting::LogUploadEvent enqueued_event =
      log_upload_event_future.Take();
  EXPECT_TRUE(enqueued_event.mutable_upload_settings()->has_origin_path());
  base::FilePath exported_file(
      enqueued_event.mutable_upload_settings()->origin_path());
  // Ensure that the resulting `exported_file` exist under target directory.
  EXPECT_EQ(exported_file.DirName(), target_dir_);

  // Check the contents of LogUploadEvent that the job enqueued.
  std::string expected_upload_parameters = test::GetExpectedUploadParameters(
      kUniqueID, exported_file.BaseName().value());
  EXPECT_EQ(
      expected_upload_parameters,
      *enqueued_event.mutable_upload_settings()->mutable_upload_parameters());

  EXPECT_TRUE(enqueued_event.has_remote_command_details());
  EXPECT_EQ(enqueued_event.remote_command_details().command_id(), kUniqueID);

  // The result payload should contain the success result code.
  base::Value::Dict expected_payload;
  expected_payload.Set("result",
                       enterprise_management::FetchSupportPacketResultCode::
                           FETCH_SUPPORT_PACKET_RESULT_SUCCESS);
  if (!session_info.pii_allowed) {
    // A note will be added to the result payload when requested PII is
    // not included in the collected logs.
    expected_payload.Set(
        "notes", base::Value::List().Append(
                     enterprise_management::FetchSupportPacketResultNote::
                         WARNING_PII_NOT_ALLOWED));
  }
  EXPECT_THAT(enqueued_event.remote_command_details().command_result_payload(),
              IsJson(expected_payload));

  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester_.ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandFetchSupportPacketTestParameterized,
    ::testing::Values(
        test::SessionInfo{TestSessionType::kManuallyLaunchedWebKioskSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{TestSessionType::kManuallyLaunchedKioskSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{TestSessionType::kAutoLaunchedWebKioskSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{TestSessionType::kAutoLaunchedKioskSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{TestSessionType::kAffiliatedUserSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{TestSessionType::kManagedGuestSession,
                          /*pii_allowed=*/false},
        test::SessionInfo{TestSessionType::kGuestSession,
                          /*pii_allowed=*/false},
        test::SessionInfo{TestSessionType::kUnaffiliatedUserSession,
                          /*pii_allowed=*/false},
        test::SessionInfo{TestSessionType::kNoSession,
                          /*pii_allowed=*/false}));

}  // namespace policy
