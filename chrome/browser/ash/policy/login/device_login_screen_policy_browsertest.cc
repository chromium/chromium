// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/test_predicate_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

namespace em = ::enterprise_management;

// Spins the loop until a notification is received from |prefs| that the value
// of |pref_name| has changed. If the notification is received before Wait()
// has been called, Wait() returns immediately and no loop is spun.
class PrefChangeWatcher {
 public:
  PrefChangeWatcher(const char* pref_name, PrefService* prefs);

  PrefChangeWatcher(const PrefChangeWatcher&) = delete;
  PrefChangeWatcher& operator=(const PrefChangeWatcher&) = delete;

  void Wait();

  void OnPrefChange();

 private:
  bool pref_changed_ = false;

  base::RunLoop run_loop_;
  PrefChangeRegistrar registrar_;
};

PrefChangeWatcher::PrefChangeWatcher(const char* pref_name,
                                     PrefService* prefs) {
  registrar_.Init(prefs);
  registrar_.Add(pref_name,
                 base::BindRepeating(&PrefChangeWatcher::OnPrefChange,
                                     base::Unretained(this)));
}

void PrefChangeWatcher::Wait() {
  if (!pref_changed_)
    run_loop_.Run();
}

void PrefChangeWatcher::OnPrefChange() {
  pref_changed_ = true;
  run_loop_.Quit();
}

}  // namespace
class DeviceLoginScreenPolicyBrowsertest : public DevicePolicyCrosBrowserTest {
 public:
  DeviceLoginScreenPolicyBrowsertest(
      const DeviceLoginScreenPolicyBrowsertest&) = delete;
  DeviceLoginScreenPolicyBrowsertest& operator=(
      const DeviceLoginScreenPolicyBrowsertest&) = delete;

 protected:
  DeviceLoginScreenPolicyBrowsertest();
  ~DeviceLoginScreenPolicyBrowsertest() override;

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name);

  bool IsPrefManaged(const char* pref_name) const;

  base::Value GetPrefValue(const char* pref_name) const;

  Profile* login_profile_ = nullptr;
};

DeviceLoginScreenPolicyBrowsertest::DeviceLoginScreenPolicyBrowsertest() {}

DeviceLoginScreenPolicyBrowsertest::~DeviceLoginScreenPolicyBrowsertest() {}

void DeviceLoginScreenPolicyBrowsertest::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  login_profile_ = ash::ProfileHelper::GetSigninProfile();
  ASSERT_TRUE(login_profile_);
  // Set the login screen profile.
  auto* accessibility_manager = ash::AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  accessibility_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());

  auto* magnification_manager = ash::MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  magnification_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());
}

void DeviceLoginScreenPolicyBrowsertest::
    RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name) {
  PrefChangeWatcher watcher(pref_name, login_profile_->GetPrefs());
  RefreshDevicePolicy();
  watcher.Wait();
}

void DeviceLoginScreenPolicyBrowsertest::SetUpCommandLine(
    base::CommandLine* command_line) {
  DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
}

bool DeviceLoginScreenPolicyBrowsertest::IsPrefManaged(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  return pref && pref->IsManaged();
}

base::Value DeviceLoginScreenPolicyBrowsertest::GetPrefValue(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  if (pref)
    return pref->GetValue()->Clone();
  else
    return base::Value();
}

IN_PROC_BROWSER_TEST_F(DeviceLoginScreenPolicyBrowsertest,
                       DeviceLoginScreenPrimaryMouseButtonSwitch) {
  // Verifies that the state of the primary mouse button on the login screen can
  // be controlled through device policy.
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);

  // Manually switch the primary mouse button to right button.
  prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, true);
  EXPECT_EQ(base::Value(true), GetPrefValue(prefs::kPrimaryMouseButtonRight));

  // Switch the primary mouse button to left button through device policy,
  // and wait for the change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_login_screen_primary_mouse_button_switch()->set_value(false);
  RefreshDevicePolicyAndWaitForPrefChange(prefs::kPrimaryMouseButtonRight);

  // Verify that the pref which controls the primary mouse button state in the
  // login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(prefs::kPrimaryMouseButtonRight));
  EXPECT_EQ(base::Value(false), GetPrefValue(prefs::kPrimaryMouseButtonRight));

  // Verify that the state of primary mouse button cannot be changed manually
  // anymore.
  prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, true);
  EXPECT_EQ(base::Value(false), GetPrefValue(prefs::kPrimaryMouseButtonRight));

  // Switch the primary mouse button to right button through device policy
  // as a recommended value, and wait for the change to take effect.
  proto.mutable_login_screen_primary_mouse_button_switch()->set_value(true);
  proto.mutable_login_screen_primary_mouse_button_switch()
      ->mutable_policy_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(prefs::kPrimaryMouseButtonRight);

  // Verify that the pref which controls the primary mouse button state in the
  // login profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(prefs::kPrimaryMouseButtonRight));
  EXPECT_EQ(base::Value(true), GetPrefValue(prefs::kPrimaryMouseButtonRight));

  // Verify that the state of primary mouse button can be enabled manually
  // again.
  prefs->SetBoolean(prefs::kPrimaryMouseButtonRight, false);
  EXPECT_EQ(base::Value(false), GetPrefValue(prefs::kPrimaryMouseButtonRight));
}

// Tests that enabling/disabling public accounts correctly reflects in the login
// UI.
IN_PROC_BROWSER_TEST_F(DeviceLoginScreenPolicyBrowsertest, DeviceLocalAccount) {
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  DeviceLocalAccountTestHelper::AddPublicSession(&proto, "test");
  RefreshDevicePolicy();

  // Wait for Gaia dialog to be hidden.
  ash::test::TestPredicateWaiter(base::BindRepeating([]() {
    return !ash::LoginScreenTestApi::IsOobeDialogVisible();
  })).Wait();
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 1);

  proto.clear_device_local_accounts();
  RefreshDevicePolicy();

  // Wait for Gaia dialog to be open.
  ash::test::TestPredicateWaiter(base::BindRepeating([]() {
    return ash::LoginScreenTestApi::IsOobeDialogVisible();
  })).Wait();
}

// Tests that adding public accounts does not close the Oobe dialog when it
// shows a screen different from the Gaia login screen.
IN_PROC_BROWSER_TEST_F(DeviceLoginScreenPolicyBrowsertest, ResetScreen) {
  ash::OobeScreenWaiter(ash::OobeBaseTest::GetFirstSigninScreen()).Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  EXPECT_EQ(ash::LoginScreenTestApi::GetUsersCount(), 0);

  // Switch to another (Reset) screen.
  ash::LoginDisplayHost::default_host()->StartWizard(ash::ResetView::kScreenId);
  ash::OobeScreenWaiter(ash::ResetView::kScreenId).Wait();

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  DeviceLocalAccountTestHelper::AddPublicSession(&proto, "test");
  RefreshDevicePolicy();

  // Wait for users to propagate.
  ash::test::TestPredicateWaiter(base::BindRepeating([]() {
    return ash::LoginScreenTestApi::GetUsersCount() > 0;
  })).Wait();

  // Oobe dialog should be open.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  // Oobe screen should not change.
  EXPECT_EQ(
      ash::LoginDisplayHost::default_host()->GetOobeUI()->current_screen(),
      ash::ResetView::kScreenId);
}

}  // namespace policy
