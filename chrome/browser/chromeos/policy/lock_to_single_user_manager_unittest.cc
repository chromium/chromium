// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/lock_to_single_user_manager.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/arc/session/arc_session_manager.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_concierge_client.h"
#include "chromeos/login/login_state/login_state.h"
#include "chromeos/login/session/session_termination_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/account_id/account_id.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/arc_util.h"
#include "components/arc/test/fake_arc_session.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/scoped_user_manager.h"

namespace policy {

class LockToSingleUserManagerTest : public BrowserWithTestWindowTest {
 public:
  LockToSingleUserManagerTest() = default;
  ~LockToSingleUserManagerTest() override = default;

  void SetUp() override {
    arc::SetArcAvailableCommandLineForTesting(
        base::CommandLine::ForCurrentProcess());
    chromeos::LoginState::Initialize();
    chromeos::CryptohomeClient::InitializeFake();
    lock_to_single_user_manager_ = std::make_unique<LockToSingleUserManager>();

    BrowserWithTestWindowTest::SetUp();

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    arc::ArcSessionManager::SetUiEnabledForTesting(false);
    arc_service_manager_ = std::make_unique<arc::ArcServiceManager>();
    arc_session_manager_ = std::make_unique<arc::ArcSessionManager>(
        std::make_unique<arc::ArcSessionRunner>(
            base::BindRepeating(arc::FakeArcSession::Create)));

    arc_service_manager_->set_browser_context(profile());

    auto setter = chromeos::DBusThreadManager::GetSetterForTesting();
    fake_concierge_client_ = new chromeos::FakeConciergeClient();
    setter->SetConciergeClient(base::WrapUnique(fake_concierge_client_));
  }

  void TearDown() override {
    arc_session_manager_->Shutdown();
    arc_service_manager_->set_browser_context(nullptr);

    BrowserWithTestWindowTest::TearDown();
    arc_session_manager_.reset();
    arc_service_manager_.reset();
    lock_to_single_user_manager_.reset();

    chromeos::CryptohomeClient::Shutdown();
    chromeos::LoginState::Shutdown();
    chromeos::DBusThreadManager::Shutdown();
  }

  void LogInUser(bool is_affiliated) {
    const AccountId account_id(AccountId::FromUserEmailGaiaId(
        profile()->GetProfileUserName(), "1234567890"));
    fake_user_manager_->AddUserWithAffiliation(account_id, is_affiliated);
    fake_user_manager_->LoginUser(account_id);
    // This step should be part of LoginUser(). There's a TODO to add it there,
    // but it breaks many tests.
    fake_user_manager_->SwitchActiveUser(account_id);

    chromeos::LoginState::Get()->SetLoggedInState(
        chromeos::LoginState::LOGGED_IN_ACTIVE,
        chromeos::LoginState::LOGGED_IN_USER_REGULAR);

    arc_session_manager_->SetProfile(profile());
    arc_session_manager_->Initialize();
  }

  void SetPolicyValue(int value) {
    settings_helper_.SetInteger(chromeos::kDeviceRebootOnUserSignout, value);
  }

  void StartArc() { arc_session_manager_->StartArcForTesting(); }
  void StartedVm(bool expect_ok = true) {
    EXPECT_EQ(
        expect_ok,
        chromeos::SessionTerminationManager::Get()->IsLockedToSingleUser());

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
    return chromeos::FakeCryptohomeClient::Get()
        ->is_device_locked_to_single_user();
  }

 private:
  chromeos::ScopedCrosSettingsTestHelper settings_helper_{
      /* create_settings_service= */ false};
  chromeos::FakeChromeUserManager* fake_user_manager_{
      new chromeos::FakeChromeUserManager()};
  user_manager::ScopedUserManager scoped_user_manager_{
      base::WrapUnique(fake_user_manager_)};
  std::unique_ptr<arc::ArcServiceManager> arc_service_manager_;
  std::unique_ptr<arc::ArcSessionManager> arc_session_manager_;
  // Required for initialization.
  chromeos::SessionTerminationManager termination_manager_;
  std::unique_ptr<LockToSingleUserManager> lock_to_single_user_manager_;
  chromeos::FakeConciergeClient* fake_concierge_client_;

  DISALLOW_COPY_AND_ASSIGN(LockToSingleUserManagerTest);
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

// Crashes on Linux. http://crbug.com/1025918
#if defined(OS_LINUX)
#define MAYBE_ConciergeStartLockTest DISABLED_ConciergeStartLockTest
#define MAYBE_PluginVmStartLockTest DISABLED_PluginVmStartLockTest
#define MAYBE_UnexpectedVmStartLockTest DISABLED_UnexpectedVmStartLockTest
#define MAYBE_DbusVmStartLockTest DISABLED_DbusVmStartLockTest
#else
#define MAYBE_ConciergeStartLockTest ConciergeStartLockTest
#define MAYBE_PluginVmStartLockTest PluginVmStartLockTest
#define MAYBE_UnexpectedVmStartLockTest UnexpectedVmStartLockTest
#define MAYBE_DbusVmStartLockTest DbusVmStartLockTest
#endif

TEST_F(LockToSingleUserManagerTest, MAYBE_ConciergeStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartConciergeVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, MAYBE_PluginVmStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartPluginVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, MAYBE_DbusVmStartLockTest) {
  SetPolicyValue(enterprise_management::DeviceRebootOnUserSignoutProto::
                     VM_STARTED_OR_ARC_SESSION);
  LogInUser(false /* is_affiliated */);
  EXPECT_FALSE(is_device_locked());
  StartDbusVm();
  StartedVm();
  EXPECT_TRUE(is_device_locked());
}

TEST_F(LockToSingleUserManagerTest, MAYBE_UnexpectedVmStartLockTest) {
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
  chromeos::FakeCryptohomeClient::Get()->set_cryptohome_error(
      cryptohome::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
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
