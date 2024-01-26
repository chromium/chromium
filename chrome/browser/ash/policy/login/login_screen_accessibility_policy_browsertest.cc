// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
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
using ::ash::AccessibilityManager;
using ::ash::MagnificationManager;

const int kDisabledScreenMagnifier = 0;
const int kFullScreenMagnifier = 1;
const int kDockedScreenMagnifier = 2;

}  // namespace

class LoginScreenAccessibilityPolicyBrowsertest
    : public DevicePolicyCrosBrowserTest {
 public:
  LoginScreenAccessibilityPolicyBrowsertest(
      const LoginScreenAccessibilityPolicyBrowsertest&) = delete;
  LoginScreenAccessibilityPolicyBrowsertest& operator=(
      const LoginScreenAccessibilityPolicyBrowsertest&) = delete;

 protected:
  LoginScreenAccessibilityPolicyBrowsertest();
  ~LoginScreenAccessibilityPolicyBrowsertest() override;

  // DevicePolicyCrosBrowserTest:
  void SetUpOnMainThread() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name);

  bool IsPrefManaged(const char* pref_name) const;

  base::Value GetPrefValue(const char* pref_name) const;

  raw_ptr<Profile, DanglingUntriaged> login_profile_ = nullptr;
};

LoginScreenAccessibilityPolicyBrowsertest::
    LoginScreenAccessibilityPolicyBrowsertest() {}

LoginScreenAccessibilityPolicyBrowsertest::
    ~LoginScreenAccessibilityPolicyBrowsertest() {}

void LoginScreenAccessibilityPolicyBrowsertest::SetUpOnMainThread() {
  DevicePolicyCrosBrowserTest::SetUpOnMainThread();
  login_profile_ = ash::ProfileHelper::GetSigninProfile();
  ASSERT_TRUE(login_profile_);
  // Set the login screen profile.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  accessibility_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());

  MagnificationManager* magnification_manager = MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  magnification_manager->SetProfileForTest(
      ash::ProfileHelper::GetSigninProfile());

  // Disable PolicyRecommendationRestorer. See https://crbug.com/1015763#c13 for
  // details.
  ash::AccessibilityController::Get()
      ->DisablePolicyRecommendationRestorerForTesting();
}

void LoginScreenAccessibilityPolicyBrowsertest::
    RefreshDevicePolicyAndWaitForPrefChange(const char* pref_name) {
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  PrefChangeRegistrar registrar;
  base::test::RepeatingTestFuture<const char*> pref_changed_future;
  registrar.Init(prefs);
  registrar.Add(pref_name, base::BindRepeating(
                               pref_changed_future.GetCallback(), pref_name));
  RefreshDevicePolicy();
  EXPECT_EQ(pref_name, pref_changed_future.Take());
}

void LoginScreenAccessibilityPolicyBrowsertest::SetUpCommandLine(
    base::CommandLine* command_line) {
  DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(ash::switches::kLoginManager);
  command_line->AppendSwitch(ash::switches::kForceLoginManagerInTests);
}

bool LoginScreenAccessibilityPolicyBrowsertest::IsPrefManaged(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  return pref && pref->IsManaged();
}

base::Value LoginScreenAccessibilityPolicyBrowsertest::GetPrefValue(
    const char* pref_name) const {
  const PrefService::Preference* pref =
      login_profile_->GetPrefs()->FindPreference(pref_name);
  if (pref)
    return pref->GetValue()->Clone();
  else
    return base::Value();
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenLargeCursorEnabled) {
  // Verifies that the state of the large cursor accessibility feature on the
  // login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Manually enable the large cursor.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_TRUE(accessibility_manager->IsLargeCursorEnabled());

  // Disable the large cursor through device policy and wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));

  // Verify that the large cursor cannot be enabled manually anymore.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       LargeCursorEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the large cursor accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenLargeCursorEnabled device policy if both of
  // DeviceLoginScreenLargeCursorEnabled and
  // DeviceLoginScreenDefaultLargeCursorEnabled have been set.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Enable the large cursor through DeviceLoginScreenLargeCursorEnabled device
  // policy, and disable it through DeviceLoginScreenDefaultLargeCursorEnabled;
  // then wait for the change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_large_cursor_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));

  // Disable the large cursor through DeviceLoginScreenLargeCursorEnabled device
  // policy, and enable it through DeviceLoginScreenDefaultLargeCursorEnabled;
  // then wait for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_large_cursor_enabled(
      false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_large_cursor_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_large_cursor_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityLargeCursorEnabled);

  // Verify that the pref which controls the large cursor in the login profile
  // is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityLargeCursorEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenSpokenFeedbackEnabled) {
  // Verifies that the state of the spoken feedback accessibility feature on the
  // login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Manually enable the spoken feedback.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Disable the spoken feedback through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));

  // Verify that the spoken feedback cannot be enabled manually anymore.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       SpokenFeedbackEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the spoken feedback accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenSpokenFeedbackEnabled device policy if both of
  // DeviceLoginScreenSpokenFeedbackEnabled and
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled have been set.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Enable the spoken feedback through DeviceLoginScreenSpokenFeedbackEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_spoken_feedback_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));

  // Disable the spoken feedback through DeviceLoginScreenSpokenFeedbackEnabled
  // device policy, and enable it through
  // DeviceLoginScreenDefaultSpokenFeedbackEnabled; then wait for the change to
  // take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_spoken_feedback_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_spoken_feedback_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_spoken_feedback_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySpokenFeedbackEnabled);

  // Verify that the pref which controls the spoken feedback in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySpokenFeedbackEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenHighContrastEnabled) {
  // Verifies that the state of the high contrast accessibility feature on the
  // login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Manually enable the high contrast.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_TRUE(accessibility_manager->IsHighContrastEnabled());

  // Disable the high contrast through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));

  // Verify that the high contrast cannot be enabled manually anymore.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       HighContrastEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the high contrast accessibility feature on the
  // login screen will be controlled only through
  // DeviceLoginScreenHighContrastEnabled device policy if both of
  // DeviceLoginScreenHighContrastEnabled and
  // DeviceLoginScreenDefaultHighContrastEnabled have been set.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Enable the high contrast through DeviceLoginScreenHighContrastEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultHighContrastEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_high_contrast_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));

  // Disable the high contrast through DeviceLoginScreenHighContrastEnabled
  // device policy, and enable it through
  // DeviceLoginScreenDefaultHighContrastEnabled; then wait for the change to
  // take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_high_contrast_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_high_contrast_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_high_contrast_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityHighContrastEnabled);

  // Verify that the pref which controls the high contrast in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityHighContrastEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityHighContrastEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenVirtualKeyboardEnabled) {
  // Verifies that the state of the virtual keyboard accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Manually enable the virtual keyboard.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_TRUE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Disable the virtual keyboard through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));

  // Verify that the virtual keyboard cannot be enabled manually anymore.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       VirtualKeyboardEnabledOverridesDefaultPolicy) {
  // Verifies that the state of the virtual keyboard accessibility feature on
  // the login screen will be controlled only through
  // DeviceLoginScreenVirtualKeyboardEnabled device policy if both of
  // DeviceLoginScreenVirtualKeyboardEnabled and
  // DeviceLoginScreenDefaultVirtualKeyboardEnabled have been set.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Enable the virtual keyboard through DeviceLoginScreenVirtualKeyboardEnabled
  // device policy, and disable it through
  // DeviceLoginScreenDefaultVirtualKeyboardEnabled; then wait for the change to
  // take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_virtual_keyboard_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_virtual_keyboard_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy and is enabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));

  // Disable the virtual keyboard through
  // DeviceLoginScreenVirtualKeyboardEnabled device policy, and enable it
  // through DeviceLoginScreenDefaultVirtualKeyboardEnabled; then wait for the
  // change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_virtual_keyboard_enabled(false);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_virtual_keyboard_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  proto.mutable_accessibility_settings()
      ->set_login_screen_default_virtual_keyboard_enabled(true);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityVirtualKeyboardEnabled);

  // Verify that the pref which controls the virtual keyboard in the login
  // profile is managed by the policy and is disabled.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityVirtualKeyboardEnabled));
}

class LoginScreenTouchVirtualKeyboardPolicyBrowsertest
    : public LoginScreenAccessibilityPolicyBrowsertest {
 private:
  // DeviceLoginScreenTouchVirtualKeyboardEnabled requires this killswitch flag
  // to work.
  base::test::ScopedFeatureList feature_list{
      ash::features::kTouchVirtualKeyboardPolicyListenPrefsAtLogin};
};

// TODO(b/307433336): Move DeviceLoginScreenDefaultVirtualKeyboardEnabled tests
// to a separate file since this is not accessibility related.

IN_PROC_BROWSER_TEST_F(LoginScreenTouchVirtualKeyboardPolicyBrowsertest,
                       DeviceLoginScreenTouchVirtualKeyboardEnabledDefault) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  // Verify keyboard disabled by default.
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());

  // Verify keyboard can be toggled by default.
  keyboard_client->SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
  keyboard_client->ClearEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_F(
    LoginScreenTouchVirtualKeyboardPolicyBrowsertest,
    DeviceLoginScreenTouchVirtualKeyboardEnabledTrueEnablesVirtualKeyboard) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_deviceloginscreentouchvirtualkeyboardenabled()->set_value(true);
  RefreshDevicePolicyAndWaitForPrefChange(prefs::kTouchVirtualKeyboardEnabled);

  // Verify the virtual keyboard cannot be disabled.
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
  keyboard_client->ClearEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_F(
    LoginScreenTouchVirtualKeyboardPolicyBrowsertest,
    DeviceLoginScreenTouchVirtualKeyboardEnabledFalseDisablesVirtualKeyboard) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_deviceloginscreentouchvirtualkeyboardenabled()->set_value(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(prefs::kTouchVirtualKeyboardEnabled);

  // Verify the virtual keyboard cannot be enabled.
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
  keyboard_client->SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenDictationEnabled) {
  // Verifies that the state of the dictation accessibility feature on the
  // login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Manually enable the dictation.
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_TRUE(accessibility_manager->IsDictationEnabled());

  // Disable the dictation through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_dictation_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityDictationEnabled);

  // Verify that the pref which controls the dictation in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityDictationEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityDictationEnabled));

  // Verify that the dictation cannot be enabled manually anymore.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Enable the dictation through device policy as a recommended value and wait
  // for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_dictation_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_dictation_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityDictationEnabled);

  // Verify that the pref which controls the dictation in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityDictationEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityDictationEnabled));

  // Verify that the dictation can be enabled manually again.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, false);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenSelectToSpeakEnabled) {
  // Verifies that the state of the select to speak accessibility feature on the
  // login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Manually enable the select to speak.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsSelectToSpeakEnabled());

  // Disable the select to speak through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_select_to_speak_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  // Verify that the pref which controls the select to speak in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilitySelectToSpeakEnabled));

  // Verify that the select to speak cannot be enabled manually anymore.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Enable the select to speak through device policy as a recommended value and
  // wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_select_to_speak_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_select_to_speak_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilitySelectToSpeakEnabled);

  // Verify that the pref which controls the select to speak in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilitySelectToSpeakEnabled));

  // Verify that the select to speak can be enabled manually again.
  accessibility_manager->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenCursorHighlightEnabled) {
  // Verifies that the state of the cursor highlight accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Manually enable the cursor highlight.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCursorHighlightEnabled());

  // Disable the cursor highlight through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_cursor_highlight_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCursorHighlightEnabled);

  // Verify that the pref which controls the cursor highlight in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityCursorHighlightEnabled));

  // Verify that the cursor highlight cannot be enabled manually anymore.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Enable the cursor highlight through device policy as a recommended value
  // and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_cursor_highlight_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_cursor_highlight_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCursorHighlightEnabled);

  // Verify that the pref which controls the cursor highlight in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityCursorHighlightEnabled));

  // Verify that the cursor highlight can be enabled manually again.
  accessibility_manager->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenCaretHighlightEnabled) {
  // Verifies that the state of the caret highlight accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Manually enable the caret highlight.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCaretHighlightEnabled());

  // Disable the caret highlight through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_caret_highlight_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCaretHighlightEnabled);

  // Verify that the pref which controls the caret highlight in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityCaretHighlightEnabled));

  // Verify that the caret highlight cannot be enabled manually anymore.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Enable the caret highlight through device policy as a recommended value and
  // wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_caret_highlight_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_caret_highlight_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityCaretHighlightEnabled);

  // Verify that the pref which controls the caret highlight in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityCaretHighlightEnabled));

  // Verify that the caret highlight can be enabled manually again.
  accessibility_manager->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenMonoAudioEnabled) {
  // Verifies that the state of the mono audio accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Manually enable the mono audio.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_TRUE(accessibility_manager->IsMonoAudioEnabled());

  // Disable the mono audio through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_mono_audio_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityMonoAudioEnabled);

  // Verify that the pref which controls the mono audio in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityMonoAudioEnabled));

  // Verify that the mono audio cannot be enabled manually anymore.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Enable the mono audio through device policy as a recommended value and wait
  // for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_mono_audio_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_mono_audio_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityMonoAudioEnabled);

  // Verify that the pref which controls the mono audio in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityMonoAudioEnabled));

  // Verify that the mono audio can be enabled manually again.
  accessibility_manager->EnableMonoAudio(false);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenAutoclickEnabled) {
  // Verifies that the state of the autoclick accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Manually enable the autoclick.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Disable the autoclick through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_autoclick_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityAutoclickEnabled);

  // Verify that the pref which controls the autoclick in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityAutoclickEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityAutoclickEnabled));

  // Verify that the autoclick cannot be enabled manually anymore.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Enable the autoclick through device policy as a recommended value and wait
  // for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_autoclick_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_autoclick_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityAutoclickEnabled);

  // Verify that the pref which controls the autoclick in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityAutoclickEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityAutoclickEnabled));

  // Verify that the autoclick can be enabled manually again.
  accessibility_manager->EnableAutoclick(false);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenStickyKeysEnabled) {
  // Verifies that the state of the sticky keys accessibility feature on
  // the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());

  // Manually enable the sticky keys.
  accessibility_manager->EnableStickyKeys(true);
  EXPECT_TRUE(accessibility_manager->IsStickyKeysEnabled());

  // Disable the sticky keys through device policy and wait for the change
  // to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_sticky_keys_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityStickyKeysEnabled);

  // Verify that the pref which controls the sticky keys in the login
  // profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityStickyKeysEnabled));

  // Verify that the sticky keys cannot be enabled manually anymore.
  accessibility_manager->EnableStickyKeys(true);
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());

  // Enable the sticky keys through device policy as a recommended value and
  // wait for the change to take effect.
  proto.mutable_accessibility_settings()->set_login_screen_sticky_keys_enabled(
      true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_sticky_keys_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityStickyKeysEnabled);

  // Verify that the pref which controls the sticky keys in the login
  // profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityStickyKeysEnabled));

  // Verify that the sticky keys can be enabled manually again.
  accessibility_manager->EnableStickyKeys(false);
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenKeyboardFocusHighlightEnabled) {
  // Verifies that the state of the keyboard focus highlight accessibility
  // feature on the login screen can be controlled through device policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  ASSERT_TRUE(accessibility_manager);
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());

  // Manually enable the keyboard focus highlight.
  accessibility_manager->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsFocusHighlightEnabled());

  // Disable the keyboard focus highlight through device policy and wait for the
  // change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_keyboard_focus_highlight_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityFocusHighlightEnabled);

  // Verify that the pref which controls the keyboard focus highlight in the
  // login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityFocusHighlightEnabled));

  // Verify that the keyboard focus highlight cannot be enabled manually
  // anymore.
  accessibility_manager->SetFocusHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());

  // Enable the keyboard focus highlight through device policy as a recommended
  // value and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_keyboard_focus_highlight_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_keyboard_focus_highlight_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityFocusHighlightEnabled);

  // Verify that the pref which controls the keyboard focus highlight in the
  // login profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityFocusHighlightEnabled));

  // Verify that the keyboard focus highlight can be enabled manually again.
  accessibility_manager->SetFocusHighlightEnabled(false);
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenFullScreenMagnifier) {
  // Verifies that the state of the full-screen magnifier accessibility
  // feature on the login screen can be controlled through device policy.
  MagnificationManager* magnification_manager = MagnificationManager::Get();
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  ASSERT_TRUE(magnification_manager);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  // Manually enable the full-screen magnifier mode.
  prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled, true);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  // Disable the screen magnifier through device policy and wait for the
  // change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_screen_magnifier_type(kDisabledScreenMagnifier);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityScreenMagnifierEnabled);

  // Verify that the pref which controls the full-screen magnifier mode in the
  // login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  // Verify that the full-screen magnifier mode cannot be enabled manually
  // anymore.
  prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled, true);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  // Enable the full-screen magnifier mode through device policy as a
  // recommended value and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_screen_magnifier_type(kFullScreenMagnifier);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_screen_magnifier_type_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityScreenMagnifierEnabled);

  // Verify that the pref which controls the full-screen magnifier mode in the
  // login profile is set to the recommended value specified in the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kAccessibilityScreenMagnifierEnabled));

  // Verify that the full-screen magnifier mode can be disabled manually.
  prefs->SetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled, false);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kAccessibilityScreenMagnifierEnabled));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenDockedMagnifier) {
  // Verifies that the state of the docked magnifier accessibility
  // feature on the login screen can be controlled through device policy.
  MagnificationManager* magnification_manager = MagnificationManager::Get();
  ASSERT_TRUE(magnification_manager);
  EXPECT_FALSE(magnification_manager->IsDockedMagnifierEnabled());

  // Manually enable the docked magnifier mode.
  magnification_manager->SetDockedMagnifierEnabled(true);
  EXPECT_TRUE(magnification_manager->IsDockedMagnifierEnabled());

  // Disable the screen magnifier through device policy and wait for the
  // change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_screen_magnifier_type(kDisabledScreenMagnifier);
  RefreshDevicePolicyAndWaitForPrefChange(ash::prefs::kDockedMagnifierEnabled);

  // Verify that the pref which controls the docked magnifier mode in the
  // login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kDockedMagnifierEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kDockedMagnifierEnabled));

  // Verify that the docked magnifier mode cannot be enabled manually
  // anymore.
  magnification_manager->SetDockedMagnifierEnabled(true);
  EXPECT_FALSE(magnification_manager->IsDockedMagnifierEnabled());

  // Enable the docked magnifier mode through device policy as a recommended
  // value and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_screen_magnifier_type(kDockedScreenMagnifier);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_screen_magnifier_type_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(ash::prefs::kDockedMagnifierEnabled);

  // Verify that the pref which controls the docked magnifier mode in the
  // login profile is being applied as recommended by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kDockedMagnifierEnabled));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kDockedMagnifierEnabled));

  // Verify that the docked magnifier mode can be enabled manually again.
  magnification_manager->SetDockedMagnifierEnabled(false);
  EXPECT_FALSE(magnification_manager->IsDockedMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenShowOptionsInSystemTrayMenu) {
  // Verifies that the visibility of the accessibility options on the login
  // screen can be controlled through device policy.
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

  // Manually show the accessibility options in tray menu.
  prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, true);
  EXPECT_TRUE(
      prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

  // Hide the accessibility options in tray menu through device policy and wait
  // for the change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()
      ->set_login_screen_show_options_in_system_tray_menu_enabled(false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kShouldAlwaysShowAccessibilityMenu);

  // Verify that the pref which controls the visibility of the accessibility
  // options tray menu in the login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kShouldAlwaysShowAccessibilityMenu));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

  // Verify that its not possible to change the visibility of the accessibility
  // options in tray menu, manually anymore.
  prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, true);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

  // Show the accessibility options in tray menu through device policy as a
  // recommended value and wait for the change to take effect.
  proto.mutable_accessibility_settings()
      ->set_login_screen_show_options_in_system_tray_menu_enabled(true);
  proto.mutable_accessibility_settings()
      ->mutable_login_screen_show_options_in_system_tray_menu_enabled_options()
      ->set_mode(em::PolicyOptions::RECOMMENDED);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kShouldAlwaysShowAccessibilityMenu);

  // Verify that the pref which controls the visibility of the accessibility
  // options in tray menu in the login profile is being applied as recommended
  // by the policy.
  EXPECT_FALSE(IsPrefManaged(ash::prefs::kShouldAlwaysShowAccessibilityMenu));
  EXPECT_EQ(base::Value(true),
            GetPrefValue(ash::prefs::kShouldAlwaysShowAccessibilityMenu));

  // Verify that the visibility of the accessibility options in tray menu can be
  // enabled manually again.
  prefs->SetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu, false);
  EXPECT_FALSE(
      prefs->GetBoolean(ash::prefs::kShouldAlwaysShowAccessibilityMenu));
}

IN_PROC_BROWSER_TEST_F(LoginScreenAccessibilityPolicyBrowsertest,
                       DeviceLoginScreenAccessibilityShortcutsEnabled) {
  // Verifies that the state of accessibility shortcuts on the login screen, can
  // be controlled using a device policy.
  PrefService* prefs = login_profile_->GetPrefs();
  ASSERT_TRUE(prefs);
  EXPECT_TRUE(prefs->GetBoolean(ash::prefs::kAccessibilityShortcutsEnabled));

  // Disable the accessibility shortcuts on login screen through device policy
  // and wait for the change to take effect.
  em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
  proto.mutable_accessibility_settings()->set_login_screen_shortcuts_enabled(
      false);
  RefreshDevicePolicyAndWaitForPrefChange(
      ash::prefs::kAccessibilityShortcutsEnabled);

  // Verify that the pref which controls whether the accessibiilty shortcuts
  // been enabled or not on the login profile is managed by the policy.
  EXPECT_TRUE(IsPrefManaged(ash::prefs::kAccessibilityShortcutsEnabled));
  EXPECT_EQ(base::Value(false),
            GetPrefValue(ash::prefs::kAccessibilityShortcutsEnabled));

  // Verify that its not possible to enable the accessibiilty shortcuts pref
  // manually.
  prefs->SetBoolean(ash::prefs::kAccessibilityShortcutsEnabled, true);
  EXPECT_FALSE(prefs->GetBoolean(ash::prefs::kAccessibilityShortcutsEnabled));
}
}  // namespace policy
