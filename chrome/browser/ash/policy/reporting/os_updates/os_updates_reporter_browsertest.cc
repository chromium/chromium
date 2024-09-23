// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_switches.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/policy/messaging_layer/proto/synced/os_events.pb.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_result_waiter.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::chromeos::MissiveClientTestObserver;
using ::reporting::Destination;
using ::reporting::Priority;
using ::reporting::Record;
using ::testing::Eq;

namespace em = enterprise_management;

namespace ash::reporting {
namespace {

constexpr char kTestUserEmail[] = "test@example.com";
constexpr char kTestAffiliationId[] = "test_affiliation_id";
constexpr char kNewPlatformVersion[] = "1235.0.0";
static const AccountId kTestAccountId = AccountId::FromUserEmailGaiaId(
    kTestUserEmail,
    signin::GetTestGaiaIdForEmail(kTestUserEmail));

struct OsUpdatesReporterBrowserTestCase {
  update_engine::Operation operation;
  bool enterprise_rollback;
};

Record GetNextOsEventsRecord(MissiveClientTestObserver* observer) {
  std::tuple<Priority, Record> enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);

  EXPECT_THAT(priority, Eq(Priority::SECURITY));
  return record;
}

class OsUpdatesReporterBrowserTest
    : public policy::DevicePolicyCrosBrowserTest {
 protected:
  OsUpdatesReporterBrowserTest() {
    login_manager_mixin_.AppendRegularUsers(1);
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        kReportOsUpdateStatus, true);
  }

  void SetUpOnMainThread() override {
    login_manager_mixin_.SetShouldLaunchBrowser(true);
    policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::DevicePolicyCrosBrowserTest::SetUpInProcessBrowserTestFixture();
    fake_update_engine_client_ =
        ash::UpdateEngineClient::InitializeFakeForTest();

    // Set up affiliation for the test user.
    auto device_policy_update = device_state_.RequestDevicePolicyUpdate();
    auto user_policy_update = user_policy_mixin_.RequestPolicyUpdate();

    device_policy_update->policy_data()->add_device_affiliation_ids(
        kTestAffiliationId);
    user_policy_update->policy_data()->add_user_affiliation_ids(
        kTestAffiliationId);
  }

  void SendFakeUpdateEngineStatus(const std::string& version,
                                  bool is_rollback,
                                  update_engine::Operation current_operation) {
    update_engine::StatusResult status;
    status.set_new_version(version);
    status.set_is_enterprise_rollback(is_rollback);
    status.set_will_powerwash_after_reboot(false);
    status.set_current_operation(current_operation);
    fake_update_engine_client_->set_default_status(status);
    fake_update_engine_client_->NotifyObserversThatStatusChanged(status);
  }

  ash::FakeSessionManagerClient* session_manager_client();

  UserPolicyMixin user_policy_mixin_{&mixin_host_, kTestAccountId};

  FakeGaiaMixin fake_gaia_mixin_{&mixin_host_};

  LoginManagerMixin login_manager_mixin_{
      &mixin_host_, LoginManagerMixin::UserList(), &fake_gaia_mixin_};

  ScopedTestingCrosSettings scoped_testing_cros_settings_;

  raw_ptr<FakeUpdateEngineClient, DanglingUntriaged>
      fake_update_engine_client_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(OsUpdatesReporterBrowserTest, ReportUpdateSuccessEvent) {
  MissiveClientTestObserver observer(Destination::OS_EVENTS);

  SendFakeUpdateEngineStatus(
      /*version=*/kNewPlatformVersion, /*is_rollback=*/false,
      /*current_operation=*/update_engine::Operation::UPDATED_NEED_REBOOT);

  const Record& update_record = GetNextOsEventsRecord(&observer);
  ASSERT_TRUE(update_record.has_source_info());
  EXPECT_THAT(update_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));
  OsEventsRecord update_record_data;
  ASSERT_TRUE(update_record_data.ParseFromString(update_record.data()));

  ASSERT_TRUE(update_record_data.has_update_event());
  EXPECT_TRUE(update_record_data.has_event_timestamp_sec());
  EXPECT_EQ(update_record_data.target_os_version(), kNewPlatformVersion);
  EXPECT_EQ(update_record_data.os_operation_type(),
            reporting::OsOperationType::SUCCESS);
}

class OsUpdatesReporterBrowserErrorTest
    : public OsUpdatesReporterBrowserTest,
      public ::testing::WithParamInterface<OsUpdatesReporterBrowserTestCase> {
 protected:
  OsUpdatesReporterBrowserErrorTest() {}
};

IN_PROC_BROWSER_TEST_P(OsUpdatesReporterBrowserErrorTest, ReportErrorEvent) {
  const auto test_case = GetParam();
  MissiveClientTestObserver observer(Destination::OS_EVENTS);

  SendFakeUpdateEngineStatus(
      /*version=*/kNewPlatformVersion,
      /*is_rollback=*/test_case.enterprise_rollback,
      /*current_operation=*/test_case.operation);

  const Record& update_record = GetNextOsEventsRecord(&observer);
  ASSERT_TRUE(update_record.has_source_info());
  EXPECT_THAT(update_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));
  OsEventsRecord update_record_data;
  ASSERT_TRUE(update_record_data.ParseFromString(update_record.data()));

  if (test_case.enterprise_rollback) {
    ASSERT_TRUE(update_record_data.has_rollback_event());
  } else {
    ASSERT_TRUE(update_record_data.has_update_event());
  }

  EXPECT_TRUE(update_record_data.has_event_timestamp_sec());
  EXPECT_EQ(update_record_data.target_os_version(), kNewPlatformVersion);
  // The reported os_operation_type is FAILURE regardless of the
  // test_case.operation.
  EXPECT_EQ(update_record_data.os_operation_type(),
            reporting::OsOperationType::FAILURE);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    OsUpdatesReporterBrowserErrorTest,
    ::testing::ValuesIn<OsUpdatesReporterBrowserTestCase>(
        {{/*operation=*/update_engine::Operation::ERROR,
          /*enterprise_rollback=*/true},
         {/*operation=*/update_engine::Operation::ERROR,
          /*enterprise_rollback=*/false},
         {/*operation=*/update_engine::Operation::REPORTING_ERROR_EVENT,
          /*enterprise_rollback=*/true},
         {/*operation=*/update_engine::Operation::REPORTING_ERROR_EVENT,
          /*enterprise_rollback=*/false}}));

class OsUpdatesReporterPowerwashBrowserTest
    : public OsUpdatesReporterBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
    DevicePolicyCrosBrowserTest::SetUp();
  }

  void InitializePolicyManager() {
    policy::BrowserPolicyConnector* const connector =
        g_browser_process->browser_policy_connector();
    connector->ScheduleServiceInitialization(0);

    policy_manager_ = g_browser_process->platform_part()
                          ->browser_policy_connector_ash()
                          ->GetDeviceCloudPolicyManager();

    DCHECK(policy_manager_);

    DCHECK(policy_manager_->core()->client());
  }

  void TriggerRemoteCommandsFetch() {
    policy::RemoteCommandsService* const remote_commands_service =
        policy_manager_->core()->remote_commands_service();
    remote_commands_service->FetchRemoteCommands();
  }

  em::RemoteCommandResult WaitForResult(int command_id) {
    em::RemoteCommandResult result =
        policy::RemoteCommandsResultWaiter(
            policy_test_server_mixin_.server()->remote_commands_state(),
            command_id)
            .WaitAndGetResult();
    return result;
  }

  int64_t AddPendingRemoteCommand(em::RemoteCommand& command) {
    return policy_test_server_mixin_.server()
        ->remote_commands_state()
        ->AddPendingRemoteCommand(command);
  }

 private:
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};

  raw_ptr<::policy::DeviceCloudPolicyManagerAsh, DanglingUntriaged>
      policy_manager_;
};

IN_PROC_BROWSER_TEST_F(OsUpdatesReporterPowerwashBrowserTest, RemotePowerwash) {
  MissiveClientTestObserver observer(Destination::OS_EVENTS);

  em::RemoteCommand command;
  command.set_type(em::RemoteCommand_Type_DEVICE_REMOTE_POWERWASH);
  int64_t command_id = AddPendingRemoteCommand(command);

  InitializePolicyManager();
  TriggerRemoteCommandsFetch();
  em::RemoteCommandResult result = WaitForResult(command_id);

  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  const Record& update_record = GetNextOsEventsRecord(&observer);
  ASSERT_TRUE(update_record.has_source_info());
  EXPECT_THAT(update_record.source_info().source(),
              Eq(::reporting::SourceInfo::ASH));
  OsEventsRecord update_record_data;
  ASSERT_TRUE(update_record_data.ParseFromString(update_record.data()));

  ASSERT_TRUE(update_record_data.has_powerwash_event());
  ASSERT_TRUE(update_record_data.powerwash_event().has_remote_request());
  EXPECT_TRUE(update_record_data.powerwash_event().remote_request());
  EXPECT_EQ(update_record_data.os_operation_type(),
            reporting::OsOperationType::INITIATED);
  EXPECT_TRUE(update_record_data.has_event_timestamp_sec());
}

}  // namespace
}  // namespace ash::reporting
