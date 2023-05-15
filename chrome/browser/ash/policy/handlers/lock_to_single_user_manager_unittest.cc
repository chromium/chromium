// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/lock_to_single_user_manager.h"

#include <memory>

#include "ash/components/arc/arc_prefs.h"
#include "ash/components/arc/metrics/arc_metrics_service.h"
#include "ash/components/arc/metrics/stability_metrics_manager.h"
#include "ash/components/arc/session/arc_service_manager.h"
#include "ash/components/arc/test/arc_util_test_support.h"
#include "ash/components/arc/test/fake_arc_session.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/ash/arc/test/test_arc_session_manager.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/ash/components/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/concierge/concierge_client.h"
#include "chromeos/ash/components/dbus/concierge/fake_concierge_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/vm_plugin_dispatcher/vm_plugin_dispatcher_client.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"

namespace policy {

class LockToSingleUserManagerTest : public BrowserWithTestWindowTest {
 public:
  LockToSingleUserManagerTest() = default;

  LockToSingleUserManagerTest(const LockToSingleUserManagerTest&) = delete;
  LockToSingleUserManagerTest& operator=(const LockToSingleUserManagerTest&) =
      delete;

  ~LockToSingleUserManagerTest() override = default;

  void SetUp() override {
    // This is required for GuestOsStabilityMonitor.
    ash::ChunneldClient::InitializeFake();
    ash::CiceroneClient::InitializeFake();
    ash::ConciergeClient::InitializeFake();
    ash::SeneschalClient::InitializeFake();

    arc::SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    ash::AnomalyDetectorClient::InitializeFake();
    ash::CryptohomeMiscClient::InitializeFake();
    ash::VmPluginDispatcherClient::InitializeFake();
    lock_to_single_user_manager_ = std::make_unique<LockToSingleUserManager>();

    BrowserWithTestWindowTest::SetUp();

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_session_manager_ = arc::CreateTestArcSessionManager(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    arc_service_manager_->set_browser_context(profile());
    arc::prefs::RegisterLocalStatePrefs(local_state_.registry());
    arc::StabilityMetricsManager::Initialize(&local_state_);
    arc::ArcMetricsService::GetForBrowserContextForTesting(profile());

    // TODO(yusukes): Stop re-creating the client here.
    ash::ConciergeClient::Shutdown();
    ash::ConciergeClient::InitializeFake(/*fake_cicerone_client=*/nullptr);
    fake_concierge_client_ = ash::FakeConciergeClient::Get();
  }

  void TearDown() override {
    arc::StabilityMetricsManager::Shutdown();
    // lock_to_single_user_manager has to be cleaned up first due to implicit
    // dependency on ArcSessionManager.
    lock_to_single_user_manager_.reset();

    arc_session_manager_->Shutdown();
    arc_session_manager_.reset();

    // Destruction order matters here.
    //
    // This line destroys profile, thus indirectly destroys
    // ArcMetricsService, since profile owns keyed services, like
    // ArcMetricsService. DTor of ArcMetricsService calls things in
    // ArcBridgeService, which is owned by ArcServiceManager. Thus
    // ArcServiceManager must still be alive at this line.
    BrowserWithTestWindowTest::TearDown();

    arc_service_manager_->set_browser_context(nullptr);
    arc_service_manager_.reset();
    ash::VmPluginDispatcherClient::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    ash::AnomalyDetectorClient::Shutdown();
    ash::SeneschalClient::Shutdown();
    ash::ConciergeClient::Shutdown();
    ash::CiceroneClient::Shutdown();
    ash::ChunneldClient::Shutdown();
  }

  void LogInUser(bool is_affiliated) {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
    // This step should be part of LoginUser(). There's a TODO to add it there,
    // but it breaks many tests.
    fake_user_manager_->SwitchActiveUser(account_id);

    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_REGULAR);

    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize();
  }

  void SetPolicyValue(int value) {
    settings_helper_.SetInteger(ash::kDeviceRebootOnUserSignout, value);
  }

  void StartArc() { arc_session_manager_->StartArcForTesting(); }
  void StartedVm(bool expect_ok = true) {
    EXPECT_EQ(expect_ok,
              ash::SessionTerminationManager::Get()->IsLockedToSingleUser());

    vm_tools::concierge::VmStartedSignal signal;  // content is irrelevant
    fake_concierge_client_->NotifyVmStarted(signal);
  }

  void StartPluginVm() {
    base::RunLoop run_loop;
    if (fake_concierge_client_->HasVmObservers())
      lock_to_single_user_manager_->OnVmStarting();
    run_loop.RunUntilIdle();
  }

  void StartConciergeVm() {
    base::RunLoop run_loop;
    if (fake_concierge_client_->HasVmObservers())
      lock_to_single_user_manager_->OnVmStarting();
    run_loop.RunUntilIdle();
  }

  void StartDbusVm() {
    base::RunLoop run_loop;
    lock_to_single_user_manager_->DbusNotifyVmStarting();
    run_loop.RunUntilIdle();
  }

  bool is_device_locked() const {
    return ash::FakeCryptohomeMiscClient::Get()
        ->is_device_locked_to_single_user();
  }

 private:
  ash::ScopedCrosSettingsTestHelper settings_helper_{
      /* create_settings_service= */ false};
  raw_ptr<ash::FakeChromeUserManager, ExperimentalAsh> fake_user_manager_{
      new ash::FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_.get())};
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  // Required for initialization.
  ash::SessionTerminationManager termination_manager_;
  std::unique_ptr<LockToSingleUserManager> lock_to_single_user_manager_;
  raw_ptr<ash::FakeConciergeClient, ExperimentalAsh> fake_concierge_client_;
  TestingPrefServiceSimple local_state_;
};

TEST_F(LockToSingleUserManagerTest, ArcSessionLockTest) {
  SetPolicyValue(
      enterprise_management::DeviceRebootOnUserSignoutProto::ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartConciergeVm();
  StartPluginVm();
  StartDbusVm();
  StartedVm(false);
  EXPECT_FALSE(is_device_locked());
  StartArc();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, ConciergeStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartConciergeVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, PluginVmStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartPluginVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, DbusVmStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartDbusVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, UnexpectedVmStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartedVm(false);
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, ArcSessionOrVmLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartArc();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, AlwaysLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);
  LogInUser(false /* is_affiliated */);
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, LateAffilitionNotificationTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);
  EXPECT_FALSE(is_device_locked());
  LogInUser(false /* is_affiliated */);
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, NeverLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::NEVER);
  LogInUser(false /* is_affiliated */);
  StartPluginVm();
  StartConciergeVm();
  StartArc();
  StartDbusVm();
  StartedVm(false);
  EXPECT_FALSE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, DbusCallErrorTest) {
  ash::FakeCryptohomeMiscClient::Get()->set_cryptohome_error(
      ::user_data_auth::CryptohomeErrorCode::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, DoesNotAffectAffiliatedUsersTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::ALWAYS);
  LogInUser(true /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, FutureTest) {
  // Unknown values should be the same as ALWAYS
  SetPolicyValue(100);
  LogInUser(false /* is_affiliated */);
  EXPECT_TRUE(is_device_locked());
}

}  // namespace policy
