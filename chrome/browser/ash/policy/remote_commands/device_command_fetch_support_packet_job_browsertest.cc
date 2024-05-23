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
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_test_helpers.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_fetch_support_packet_job_test_util.h"
#include "chrome/browser/ash/policy/remote_commands/user_session_type_test_util.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/log_upload_event.pb.h"
#include "chrome/browser/policy/messaging_layer/public/report_client_test_util.h"
#include "chrome/browser/support_tool/data_collection_module.pb.h"
#include "chrome/browser/support_tool/support_tool_util.h"
#include "chrome_device_policy.pb.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/storage/test_storage_module.h"
#include "components/user_manager/user_manager.h"
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

constexpr char kUnaffiliatedUser[] = "user@gmail.com";
constexpr char kUnaffiliatedGaiaID[] = "11111";

constexpr char kAffiliatedUser[] = "user@example.com";
constexpr char kAffiliatedGaiaID[] = "22222";

// Use a number larger than int32 to catch truncation errors.
const int64_t kInitialCommandId = (1LL << 35) + 1;

template <typename BaseBrowserTest>
class DeviceCommandFetchSupportPacketBrowserTestBase : public BaseBrowserTest {
  static_assert(
      std::is_base_of_v<MixinBasedInProcessBrowserTest, BaseBrowserTest>,
      "Must be MixinBasedInProcessBrowserTest");

 protected:
  void SetUpOnMainThread() override {
    // Reporting test environment needs to be created before the browser
    // creation is completed.
    reporting_test_storage_ =
        base::MakeRefCounted<reporting::test::TestStorageModule>();

    reporting_test_enviroment_ =
        reporting::ReportingClient::TestEnvironment::CreateWithStorageModule(
            reporting_test_storage_);

    BaseBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    BaseBrowserTest::TearDownOnMainThread();

    reporting_test_enviroment_.reset();
    reporting_test_storage_.reset();
  }

  void SetUpInProcessBrowserTestFixture() override {
    BaseBrowserTest::SetUpInProcessBrowserTestFixture();

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
    DeviceCommandFetchSupportPacketJob::SetTargetDirForTesting(nullptr);
    BaseBrowserTest::TearDownInProcessBrowserTestFixture();
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
    em::ChromeDeviceSettingsProto& proto(
        policy_helper_.device_policy()->payload());
    proto.mutable_device_log_upload_settings()->set_system_log_upload_enabled(
        enabled);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {ash::kSystemLogUploadEnabled});
    policy_test_server_mixin_.UpdateDevicePolicy(proto);
  }

  // Checks that the contents of `event` are as expected for the given
  // `command_id`. Verifies that `event` contains a command result payload JSON
  // and the command result payload JSON has a note about requested PII not
  // being able to be collected in "notes" field.
  void CheckLogUploadEventContents(const ash::reporting::LogUploadEvent& event,
                                   int64_t command_id,
                                   bool expect_pii_note) {
    EXPECT_TRUE(event.upload_settings().has_origin_path());
    base::FilePath exported_file(event.upload_settings().origin_path());
    // Ensure that the resulting `exported_file` exist under target directory.
    EXPECT_EQ(exported_file.DirName(), target_dir());
    EXPECT_TRUE(event.has_remote_command_details());
    EXPECT_EQ(event.remote_command_details().command_id(), command_id);

    std::string expected_upload_parameters = test::GetExpectedUploadParameters(
        command_id, exported_file.BaseName().value());
    EXPECT_EQ(expected_upload_parameters,
              event.upload_settings().upload_parameters());

    // The result payload should contain the success result code.
    base::Value::Dict expected_payload;
    expected_payload.Set("result",
                         enterprise_management::FetchSupportPacketResultCode::
                             FETCH_SUPPORT_PACKET_RESULT_SUCCESS);
    if (expect_pii_note) {
      // A note will be added to the result payload when requested PII is
      // not included in the collected logs.
      expected_payload.Set(
          "notes", base::Value::List().Append(
                       enterprise_management::FetchSupportPacketResultNote::
                           WARNING_PII_NOT_ALLOWED));
    }
    EXPECT_THAT(event.remote_command_details().command_result_payload(),
                IsJson(expected_payload));
  }

  const base::HistogramTester& histogram_tester() { return histogram_tester_; }
  scoped_refptr<reporting::test::TestStorageModule> reporting_storage() {
    return reporting_test_storage_;
  }
  const base::FilePath& target_dir() { return target_dir_; }

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }

 private:
  scoped_refptr<reporting::test::TestStorageModule> reporting_test_storage_;
  std::unique_ptr<reporting::ReportingClient::TestEnvironment>
      reporting_test_enviroment_;

  ash::system::FakeStatisticsProvider statistics_provider_;
  base::HistogramTester histogram_tester_;

  base::ScopedTempDir scoped_temp_dir_;
  base::FilePath target_dir_;

  DevicePolicyCrosTestHelper policy_helper_;

  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{
      &(BaseBrowserTest::mixin_host_)};
  RemoteCommandsServiceMixin remote_commands_service_mixin_{
      BaseBrowserTest::mixin_host_, policy_test_server_mixin_};
};

// This is an intermediate base class for testing FETCH_SUPPORT_PACKET on
// different session types (e.g. kiosk and MGS). This class is also used for
// testing FailWhenLogUploadDisabled case.
class DeviceCommandFetchSupportPacketBrowserTest
    : public DeviceCommandFetchSupportPacketBrowserTestBase<
          DevicePolicyCrosBrowserTest> {};

// Tests FETCH_SUPPORT_PACKET command on different session types
// (affiliated/unaffiliated user sessions, managed guest session). For other
// session types, please see
// DeviceCommandFetchSupportPacketBrowserTestAutoLaunchKioskSession and
// DeviceCommandFetchSupportPacketBrowserTestManualKioskSession tests.
class DeviceCommandFetchSupportPacketBrowserTestParameterized
    : public DeviceCommandFetchSupportPacketBrowserTest,
      public ::testing::WithParamInterface<test::SessionInfo> {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandFetchSupportPacketBrowserTest::
        SetUpInProcessBrowserTestFixture();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();
  }

  void StartSession(test::TestSessionType session_type) {
    switch (session_type) {
      case test::TestSessionType::kNoSession:
        // Do nothing and stay in login screen.
        ASSERT_FALSE(ash::LoginState::Get()->IsUserLoggedIn());
        break;
      case test::TestSessionType::kAffiliatedUserSession:
        LoginUserAndSetAffiliation(affiliated_user_, /*is_affiliated=*/true);
        break;
      case test::TestSessionType::kUnaffiliatedUserSession:
        LoginUserAndSetAffiliation(unaffiliated_user_, /*is_affiliated=*/false);
        break;
      case test::TestSessionType::kManagedGuestSession:
        ASSERT_NO_FATAL_FAILURE(LaunchMGS());
        break;
      default:
        NOTREACHED_IN_MIGRATION();
    }
  }

 private:
  void LoginUserAndSetAffiliation(
      const ash::LoginManagerMixin::TestUserInfo& user,
      bool is_affiliated) {
    login_manager_mixin_.LoginWithDefaultContext(user);
    login_manager_mixin_.WaitForActiveSession();
    // UserManager is initialized as ash::FakeChromeUserManager by the included
    // mixins so it's safe to cast.
    ash::FakeChromeUserManager* fake_user_manager =
        static_cast<ash::FakeChromeUserManager*>(
            user_manager::UserManager::Get());
    fake_user_manager->SetUserAffiliationForTesting(user.account_id,
                                                    is_affiliated);
  }

  void LaunchMGS() {
    // Set up MGS auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(
        policy_helper()->device_policy()->payload());
    ash::AppendAutoLaunchManagedGuestSessionAccount(&proto);

    policy_helper()->RefreshDevicePolicy();
    ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();
    ASSERT_TRUE(ash::LoginState::Get()->IsManagedGuestSessionUser());
  }

  const ash::LoginManagerMixin::TestUserInfo unaffiliated_user_{
      AccountId::FromUserEmailGaiaId(kUnaffiliatedUser, kUnaffiliatedGaiaID)};

  const ash::LoginManagerMixin::TestUserInfo affiliated_user_{
      AccountId::FromUserEmailGaiaId(kAffiliatedUser, kAffiliatedGaiaID)};

  ash::UserPolicyMixin user_policy_mixin_{&mixin_host_,
                                          affiliated_user_.account_id};
  ash::LoginManagerMixin login_manager_mixin_{
      &mixin_host_,
      {affiliated_user_, unaffiliated_user_}};
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandFetchSupportPacketBrowserTestParameterized,
    ::testing::Values(
        test::SessionInfo{test::TestSessionType::kNoSession,
                          /*pii_allowed=*/false},
        test::SessionInfo{test::TestSessionType::kAffiliatedUserSession,
                          /*pii_allowed=*/true},
        test::SessionInfo{test::TestSessionType::kUnaffiliatedUserSession,
                          /*pii_allowed=*/false},
        test::SessionInfo{test::TestSessionType::kManagedGuestSession,
                          /*pii_allowed=*/false}));

class DeviceCommandFetchSupportPacketBrowserTestAutoLaunchKioskSession
    : public DeviceCommandFetchSupportPacketBrowserTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandFetchSupportPacketBrowserTest::
        SetUpInProcessBrowserTestFixture();
    // Set up kiosk auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(
        policy_helper()->device_policy()->payload());
    ash::KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    policy_helper()->RefreshDevicePolicy();
  }

 private:
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_, {}};
};

class DeviceCommandFetchSupportPacketBrowserTestManualKioskSession
    : public DeviceCommandFetchSupportPacketBrowserTestBase<
          ash::KioskBaseTest> {
 protected:
  DeviceCommandFetchSupportPacketBrowserTestManualKioskSession() = default;

  void SetLogUploadEnabledPolicy(bool enabled) {
    em::ChromeDeviceSettingsProto& proto(
        policy_helper()->device_policy()->payload());
    proto.mutable_device_log_upload_settings()->set_system_log_upload_enabled(
        true);
    policy_helper()->RefreshDevicePolicy();
    settings_helper_.SetBoolean(ash::kSystemLogUploadEnabled, true);
  }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

}  // namespace

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

IN_PROC_BROWSER_TEST_P(DeviceCommandFetchSupportPacketBrowserTestParameterized,
                       SuccessCommandRequestWithoutPii) {
  SetLogUploadEnabledPolicy(true);
  ASSERT_NO_FATAL_FAILURE(StartSession(GetParam().session_type));

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
  CheckLogUploadEventContents(event, command_id, /*expect_pii_note=*/false);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_P(DeviceCommandFetchSupportPacketBrowserTestParameterized,
                       SuccessCommandRequestWithPii) {
  SetLogUploadEnabledPolicy(true);
  ASSERT_NO_FATAL_FAILURE(StartSession(GetParam().session_type));

  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());

  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      GetAllAvailableDataCollectorsOnDevice(), {support_tool::PiiType::EMAIL}));
  ASSERT_TRUE(payload.has_value());
  int64_t command_id = WaitForCommandExecution(
      RemoteCommandBuilder()
          .SetType(em::RemoteCommand::FETCH_SUPPORT_PACKET)
          .SetPayload(payload.value())
          .Build());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  CheckLogUploadEventContents(
      event, command_id,
      // Expect a note if PII is not allowed in the session.
      /*expect_pii_note=*/!GetParam().pii_allowed);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeviceCommandFetchSupportPacketBrowserTestAutoLaunchKioskSession,
    SuccessCommandRequestWithoutPii) {
  ASSERT_TRUE(chromeos::IsKioskSession());

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
  CheckLogUploadEventContents(event, command_id, /*expect_pii_note=*/false);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeviceCommandFetchSupportPacketBrowserTestAutoLaunchKioskSession,
    SuccessCommandRequestWithPii) {
  SetLogUploadEnabledPolicy(true);

  ASSERT_TRUE(chromeos::IsKioskSession());

  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());

  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      GetAllAvailableDataCollectorsOnDevice(), {support_tool::PiiType::EMAIL}));
  ASSERT_TRUE(payload.has_value());
  int64_t command_id = WaitForCommandExecution(
      RemoteCommandBuilder()
          .SetType(em::RemoteCommand::FETCH_SUPPORT_PACKET)
          .SetPayload(payload.value())
          .Build());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  CheckLogUploadEventContents(event, command_id,
                              // PII is allowed on kiosk sessions.
                              /*expect_pii_note=*/false);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeviceCommandFetchSupportPacketBrowserTestManualKioskSession,
    SuccessCommandRequestWithoutPii) {
  SetLogUploadEnabledPolicy(true);

  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForAppLaunchWithOptions(false /* check launch data */,
                              false /* terminate app */,
                              true /* keep app open */);
  ASSERT_TRUE(chromeos::IsKioskSession());

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
  CheckLogUploadEventContents(event, command_id, /*expect_pii_note=*/false);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

IN_PROC_BROWSER_TEST_F(
    DeviceCommandFetchSupportPacketBrowserTestManualKioskSession,
    SuccessCommandRequestWithPii) {
  SetLogUploadEnabledPolicy(true);

  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForAppLaunchWithOptions(false /* check launch data */,
                              false /* terminate app */,
                              true /* keep app open */);
  ASSERT_TRUE(chromeos::IsKioskSession());

  base::test::TestFuture<ash::reporting::LogUploadEvent>
      log_upload_event_future;
  test::CaptureUpcomingLogUploadEventOnReportingStorage(
      reporting_storage(), log_upload_event_future.GetRepeatingCallback());

  auto payload = base::WriteJson(test::GetFetchSupportPacketCommandPayloadDict(
      GetAllAvailableDataCollectorsOnDevice(), {support_tool::PiiType::EMAIL}));
  ASSERT_TRUE(payload.has_value());
  int64_t command_id = WaitForCommandExecution(
      RemoteCommandBuilder()
          .SetType(em::RemoteCommand::FETCH_SUPPORT_PACKET)
          .SetPayload(payload.value())
          .Build());

  ash::reporting::LogUploadEvent event = log_upload_event_future.Take();
  CheckLogUploadEventContents(event, command_id,
                              // PII is allowed on kiosk sessions.
                              /*expect_pii_note=*/false);

  // Check contents of the resulting file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking_for_test;
    int64_t file_size;
    base::FilePath exported_file(event.upload_settings().origin_path());
    ASSERT_TRUE(base::GetFileSize(exported_file, &file_size));
    EXPECT_GT(file_size, 0);
  }

  histogram_tester().ExpectUniqueSample(
      kFetchSupportPacketFailureHistogramName,
      EnterpriseFetchSupportPacketFailureType::kNoFailure, 1);
}

}  // namespace policy
