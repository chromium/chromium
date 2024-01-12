// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {

namespace em = ::enterprise_management;
using ::ash::AccessibilityManager;
using ::ash::MagnificationManager;

const em::AccessibilitySettingsProto_ScreenMagnifierType kFullScreenMagnifier =
    em::AccessibilitySettingsProto_ScreenMagnifierType_SCREEN_MAGNIFIER_TYPE_FULL;

}  // namespace

class LoginScreenDefaultPolicyBrowsertestBase
    : public DevicePolicyCrosBrowserTest {
 public:
  LoginScreenDefaultPolicyBrowsertestBase(
      const LoginScreenDefaultPolicyBrowsertestBase&) = delete;
  LoginScreenDefaultPolicyBrowsertestBase& operator=(
      const LoginScreenDefaultPolicyBrowsertestBase&) = delete;

 protected:
  LoginScreenDefaultPolicyBrowsertestBase();
  ~LoginScreenDefaultPolicyBrowsertestBase() override;

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override;

  void RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name);

  raw_ptr<Profile, DanglingUntriaged> login_profile_;
};

class LoginScreenDefaultPolicyLoginScreenBrowsertest
    : public LoginScreenDefaultPolicyBrowsertestBase {
 public:
  LoginScreenDefaultPolicyLoginScreenBrowsertest(
      const LoginScreenDefaultPolicyLoginScreenBrowsertest&) = delete;
  LoginScreenDefaultPolicyLoginScreenBrowsertest& operator=(
      const LoginScreenDefaultPolicyLoginScreenBrowsertest&) = delete;

 protected:
  LoginScreenDefaultPolicyLoginScreenBrowsertest();
  ~LoginScreenDefaultPolicyLoginScreenBrowsertest() override;

  // LoginScreenDefaultPolicyBrowsertestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  void VerifyPrefFollowsRecommendation(const char* pref_name,
                                       const base::Value& recommended_value);
};

class LoginScreenDefaultPolicyInSessionBrowsertest
    : public LoginScreenDefaultPolicyBrowsertestBase {
 public:
  LoginScreenDefaultPolicyInSessionBrowsertest(
      const LoginScreenDefaultPolicyInSessionBrowsertest&) = delete;
  LoginScreenDefaultPolicyInSessionBrowsertest& operator=(
      const LoginScreenDefaultPolicyInSessionBrowsertest&) = delete;

 protected:
  LoginScreenDefaultPolicyInSessionBrowsertest();
  ~LoginScreenDefaultPolicyInSessionBrowsertest() override;

  // LoginScreenDefaultPolicyBrowsertestBase:
  void SetUpOnMainThread() override;

  void VerifyPrefFollowsDefault(const char* pref_name);
};

LoginScreenDefaultPolicyBrowsertestBase::
    LoginScreenDefaultPolicyBrowsertestBase()
    : login_profile_(nullptr) {}

LoginScreenDefaultPolicyBrowsertestBase::
    ~LoginScreenDefaultPolicyBrowsertestBase() {}

void LoginScreenDefaultPolicyBrowsertestBase::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  login_profile_ = ash::ProfileHelper::GetSigninProfile();
  ASSERT_TRUE(login_profile_);
}

void LoginScreenDefaultPolicyBrowsertestBase::
    RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name) {
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  PrefChangeRegistrar registrar;
  base::test::TestFuture<const char*> pref_changed_future;
  registrar.Init(prefs);
  registrar.Add(pref_name,
                base::BindRepeating(pref_changed_future.GetRepeatingCallback(),
                                    pref_name));
  RefreshDevicePolicy();
  EXPECT_EQ(pref_name, pref_changed_future.Take());
}

LoginScreenDefaultPolicyLoginScreenBrowsertest::
    LoginScreenDefaultPolicyLoginScreenBrowsertest() {}

LoginScreenDefaultPolicyLoginScreenBrowsertest::
    ~LoginScreenDefaultPolicyLoginScreenBrowsertest() {}

void LoginScreenDefaultPolicyLoginScreenBrowsertest::SetUpCommandLine(
    base::CommandLine* command_line) {
  LoginScreenDefaultPolicyBrowsertestBase::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
}

void LoginScreenDefaultPolicyLoginScreenBrowsertest::SetUpOnMainThread() {
  LoginScreenDefaultPolicyBrowsertestBase::SetUpOnMainThread();

  // Set the login screen profile.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  accessibility_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());

  MagnificationManager* magnification_manager = MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  magnification_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());
}

void LoginScreenDefaultPolicyLoginScreenBrowsertest::TearDownOnMainThread() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&chrome::AttemptExit));
  base::RunLoop().RunUntilIdle();
  LoginScreenDefaultPolicyBrowsertestBase::TearDownOnMainThread();
}

void LoginScreenDefaultPolicyLoginScreenBrowsertest::
    VerifyPrefFollowsRecommendation(const char* pref_name,
                                    const base::Value& recommended_value) {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_FALSE(pref->IsDefaultValue());
  EXPECT_EQ(recommended_value, *pref->GetValue());
  EXPECT_EQ(recommended_value, *pref->GetRecommendedValue());
}

LoginScreenDefaultPolicyInSessionBrowsertest::
    LoginScreenDefaultPolicyInSessionBrowsertest() {}

LoginScreenDefaultPolicyInSessionBrowsertest::
    ~LoginScreenDefaultPolicyInSessionBrowsertest() {}

void LoginScreenDefaultPolicyInSessionBrowsertest::SetUpOnMainThread() {
  LoginScreenDefaultPolicyBrowsertestBase::SetUpOnMainThread();
}

void LoginScreenDefaultPolicyInSessionBrowsertest::VerifyPrefFollowsDefault(
    const char* pref_name) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  ASSERT_TRUE(profile);
  const PrefService::Preference* pref =
      profile->GetPrefs()->FindPreference(pref_name);
  ASSERT_TRUE(pref);
  EXPECT_FALSE(pref->IsManaged());
  EXPECT_TRUE(pref->IsDefaultValue());
  EXPECT_FALSE(pref->GetRecommendedValue());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyLoginScreenBrowsertest,
                       DeviceLoginScreenDefaultLargeCursorEnabled) {
  // Verifies that the default state of the large cursor accessibility feature
  // on the login screen can be controlled through device policy.

  // Enable the large cursor through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // has changed to the policy-supplied default.
  VerifyPrefFollowsRecommendation(ash::prefs::kAccessibilityLargeCursorEnabled,
                                  base::Value(true));

  // Verify that the large cursor is enabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_TRUE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyLoginScreenBrowsertest,
                       DeviceLoginScreenDefaultSpokenFeedbackEnabled) {
  // Verifies that the default state of the spoken feedback accessibility
  // feature on the login screen can be controlled through device policy.

  // Enable spoken feedback through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls spoken feedback in the login profile
  // has changed to the policy-supplied default.
  VerifyPrefFollowsRecommendation(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled, base::Value(true));

  // Verify that spoken feedback is enabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyLoginScreenBrowsertest,
                       DeviceLoginScreenDefaultHighContrastEnabled) {
  // Verifies that the default state of the high contrast mode accessibility
  // feature on the login screen can be controlled through device policy.

  // Enable high contrast mode through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls high contrast mode in the login profile
  // has changed to the policy-supplied default.
  VerifyPrefFollowsRecommendation(ash::prefs::kAccessibilityHighContrastEnabled,
                                  base::Value(true));

  // Verify that high contrast mode is enabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_TRUE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyLoginScreenBrowsertest,
                       DeviceLoginScreenDefaultScreenMagnifierEnabled) {
  // Verifies that the screen magnifier enabled on the login screen can be
  // controlled through device policy.

  // Set the screen magnifier through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_screen_magnifier_type(kFullScreenMagnifier);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityScreenMagnifierEnabled);

  // Verify that the prefs which control the screen magnifier have changed
  // to the policy-supplied default.
  VerifyPrefFollowsRecommendation(
      ash::prefs::kAccessibilityScreenMagnifierEnabled, base::Value(true));

  // Verify that the full-screen magnifier is enabled.
  MagnificationManager* magnification_manager = MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyInSessionBrowsertest,
                       DeviceLoginScreenDefaultLargeCursorEnabled) {
  // Verifies that changing the default state of the large cursor accessibility
  // feature on the login screen through policy does not affect its state in a
  // session.

  // Enable the large cursor through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the session is
  // unchanged.
  VerifyPrefFollowsDefault(ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the large cursor is disabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyInSessionBrowsertest,
                       DeviceLoginScreenDefaultSpokenFeedbackEnabled) {
  // Verifies that changing the default state of the spoken feedback
  // accessibility feature on the login screen through policy does not affect
  // its state in a session.

  // Enable spoken feedback through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the session is
  // unchanged.
  VerifyPrefFollowsDefault(ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that spoken feedback is disabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyInSessionBrowsertest,
                       DeviceLoginScreenDefaultHighContrastEnabled) {
  // Verifies that changing the default state of the high contrast mode
  // accessibility feature on the login screen through policy does not affect
  // its state in a session.

  // Enable high contrast mode through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls high contrast mode in the session is
  // unchanged.
  VerifyPrefFollowsDefault(ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that high contrast mode is disabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyInSessionBrowsertest,
                       DeviceLoginScreenDefaultScreenMagnifierEnabled) {
  // Verifies that changing the screen magnifier enabled on the login screen
  // through policy does not affect its state in a session.

  // Set the screen magnifier through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_screen_magnifier_type(kFullScreenMagnifier);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityScreenMagnifierEnabled);

  // Verify that the prefs which control the screen magnifier in the session are
  // unchanged.
  VerifyPrefFollowsDefault(ash::prefs::kAccessibilityScreenMagnifierEnabled);

  // Verify that the screen magnifier is disabled.
  MagnificationManager* magnification_manager = MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenDefaultPolicyLoginScreenBrowsertest,
                       DeviceLoginScreenDefaultVirtualKeyboardEnabled) {
  // Verifies that the default state of the on-screen keyboard accessibility
  // feature on the login screen can be controlled through device policy.

  // Enable the on-screen keyboard through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_virtual_keyboard_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the on-screen keyboard in the login
  // profile has changed to the policy-supplied default.
  VerifyPrefFollowsRecommendation(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled, base::Value(true));

  // Verify that the on-screen keyboard is enabled.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_TRUE(accessibility_manager->IsVirtualKeyboardEnabled());
}

}  // namespace policy
