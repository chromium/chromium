// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/browser/ash/accessibility/magnifier_type.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/browser.h"
#include "components/prefs/pref_service.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <tuple>

#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform.h"
#endif

namespace policy {

#if BUILDFLAG(IS_CHROMEOS_ASH)

using ::ash::AccessibilityManager;
using ::ash::MagnificationManager;
using ::ash::MagnifierType;

namespace {

void SetEnableFlag(const keyboard::KeyboardEnableFlag& flag) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  keyboard_client->SetEnableFlag(flag);
}

void ClearEnableFlag(const keyboard::KeyboardEnableFlag& flag) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  keyboard_client->ClearEnableFlag(flag);
}

}  // namespace

class AccessibilityPolicyTest : public PolicyTest {};

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, LargeCursorEnabled) {
  // Verifies that the large cursor accessibility feature can be controlled
  // through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Manually enable the large cursor.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_TRUE(accessibility_manager->IsLargeCursorEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kLargeCursorEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());

  // Verify that the large cursor cannot be enabled manually anymore.
  accessibility_manager->EnableLargeCursor(true);
  EXPECT_FALSE(accessibility_manager->IsLargeCursorEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, SpokenFeedbackEnabled) {
  // Verifies that the spoken feedback accessibility feature can be controlled
  // through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Manually enable spoken feedback.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_TRUE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kSpokenFeedbackEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());

  // Verify that spoken feedback cannot be enabled manually anymore.
  accessibility_manager->EnableSpokenFeedback(true);
  EXPECT_FALSE(accessibility_manager->IsSpokenFeedbackEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, HighContrastEnabled) {
  // Verifies that the high contrast mode accessibility feature can be
  // controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Manually enable high contrast mode.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_TRUE(accessibility_manager->IsHighContrastEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kHighContrastEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());

  // Verify that high contrast mode cannot be enabled manually anymore.
  accessibility_manager->EnableHighContrast(true);
  EXPECT_FALSE(accessibility_manager->IsHighContrastEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, ScreenMagnifierTypeNone) {
  // Verifies that the screen magnifier can be disabled through policy.
  MagnificationManager* magnification_manager = MagnificationManager::Get();

  // Manually enable the full-screen magnifier.
  magnification_manager->SetMagnifierEnabled(true);
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(0), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());

  // Verify that the screen magnifier cannot be enabled manually anymore.
  magnification_manager->SetMagnifierEnabled(true);
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());
  // Verify that the docked magnifier cannot be enabled manually anymore.
  magnification_manager->SetDockedMagnifierEnabled(true);
  EXPECT_FALSE(magnification_manager->IsDockedMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, ScreenMagnifierTypeFull) {
  // Verifies that the full-screen magnifier can be enabled through policy.
  MagnificationManager* magnification_manager = MagnificationManager::Get();

  // Verify that the screen magnifier is initially disabled.
  EXPECT_FALSE(magnification_manager->IsMagnifierEnabled());

  // Verify that policy can enable the full-screen magnifier.
  PolicyMap policies;
  policies.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(MagnifierType::kFull)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());

  // Verify that the screen magnifier cannot be disabled manually anymore.
  magnification_manager->SetMagnifierEnabled(false);
  EXPECT_TRUE(magnification_manager->IsMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, ScreenMagnifierTypeDocked) {
  // Verifies that the docked magnifier accessibility feature can be
  // controlled through policy.
  MagnificationManager* magnification_manager = MagnificationManager::Get();

  // Verify that the docked magnifier is initially disabled
  EXPECT_FALSE(magnification_manager->IsDockedMagnifierEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kScreenMagnifierType, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               base::Value(static_cast<int>(MagnifierType::kDocked)), nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(magnification_manager->IsDockedMagnifierEnabled());

  // Verify that the docked magnifier cannot be disabled manually anymore.
  magnification_manager->SetDockedMagnifierEnabled(false);
  EXPECT_TRUE(magnification_manager->IsDockedMagnifierEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest,
                       AccessibilityVirtualKeyboardEnabled) {
  // Verifies that the on-screen keyboard accessibility feature can be
  // controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Manually enable the on-screen keyboard.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_TRUE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());

  // Verify that the on-screen keyboard cannot be enabled manually anymore.
  accessibility_manager->EnableVirtualKeyboard(true);
  EXPECT_FALSE(accessibility_manager->IsVirtualKeyboardEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, StickyKeysEnabled) {
  // Verifies that the sticky keys accessibility feature can be
  // controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Verify that the sticky keys is initially disabled
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());

  // Manually enable the sticky keys.
  accessibility_manager->EnableStickyKeys(true);
  EXPECT_TRUE(accessibility_manager->IsStickyKeysEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kStickyKeysEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());

  // Verify that the sticky keys cannot be enabled manually anymore.
  accessibility_manager->EnableStickyKeys(true);
  EXPECT_FALSE(accessibility_manager->IsStickyKeysEnabled());
}

// TODO(b/307433336): Remove this once the flag is enabled by default.
// TODO(b/307433336): Move these tests to a separate file since these are not
// accessibility related.
class AccessibilityPolicyTouchVirtualKeyboardEnabledTest
    : public AccessibilityPolicyTest,
      public testing::WithParamInterface<bool> {
 public:
  AccessibilityPolicyTouchVirtualKeyboardEnabledTest() {
    feature_list_.InitWithFeatureState(
        ash::features::kTouchVirtualKeyboardPolicyListenPrefsAtLogin,
        GetParam());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(,
                         AccessibilityPolicyTouchVirtualKeyboardEnabledTest,
                         ::testing::Values(true, false));

IN_PROC_BROWSER_TEST_P(AccessibilityPolicyTouchVirtualKeyboardEnabledTest,
                       TouchVirtualKeyboardEnabledDefault) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  // Verify keyboard disabled by default.
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());

  // Verify keyboard can be toggled by default.
  SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
  ClearEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_P(AccessibilityPolicyTouchVirtualKeyboardEnabledTest,
                       TouchVirtualKeyboardEnabledTrueEnablesVirtualKeyboard) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  // Verify enabling the policy takes effect immediately and that that user
  // cannot disable the keyboard..
  PolicyMap policies;
  policies.Set(key::kTouchVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
  ClearEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_TRUE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_P(
    AccessibilityPolicyTouchVirtualKeyboardEnabledTest,
    TouchVirtualKeyboardEnabledFalseDisablesVirtualKeyboard) {
  auto* keyboard_client = ChromeKeyboardControllerClient::Get();
  ASSERT_TRUE(keyboard_client);

  // Verify that disabling the policy takes effect immediately and that the user
  // cannot enable the keyboard.
  PolicyMap policies;
  policies.Set(key::kTouchVirtualKeyboardEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
  SetEnableFlag(keyboard::KeyboardEnableFlag::kTouchEnabled);
  EXPECT_FALSE(keyboard_client->is_keyboard_enabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, SelectToSpeakEnabled) {
  // Verifies that the select to speak accessibility feature can be
  // controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Verify that the select to speak is initially disabled
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Manually enable the select to speak.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsSelectToSpeakEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kSelectToSpeakEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());

  // Verify that the select to speak cannot be enabled manually anymore.
  accessibility_manager->SetSelectToSpeakEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsSelectToSpeakEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, DictationEnabled) {
  // Verifies that the dictation accessibility feature can be
  // controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  PrefService* prefs = browser()->profile()->GetPrefs();

  // Verify that the dictation is initially disabled
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Manually enable the dictation.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_TRUE(accessibility_manager->IsDictationEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kDictationEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());

  // Verify that the dictation cannot be enabled manually anymore.
  prefs->SetBoolean(ash::prefs::kAccessibilityDictationEnabled, true);
  EXPECT_FALSE(accessibility_manager->IsDictationEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, KeyboardFocusHighlightEnabled) {
  // Verifies that the keyboard focus highlight objects accessibility feature
  // can be controlled through policy.
  AccessibilityManager* const accessibility_manager =
      AccessibilityManager::Get();

  // Verify that the keyboard focus highlight objects is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());

  // Manually enable the keyboard focus highlight objects.
  accessibility_manager->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsFocusHighlightEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kKeyboardFocusHighlightEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());

  // Verify that the keyboard focus highlight objects cannot be enabled manually
  // anymore.
  accessibility_manager->SetFocusHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsFocusHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, CursorHighlightEnabled) {
  // Verifies that the cursor highlight accessibility feature accessibility
  // feature can be controlled through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Verify that the cursor highlight is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Manually enable the cursor highlight.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCursorHighlightEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kCursorHighlightEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());

  // Verify that the cursor highlight cannot be enabled manually anymore.
  accessibility_manager->SetCursorHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCursorHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, CaretHighlightEnabled) {
  // Verifies that the caret highlight accessibility feature can be controlled
  // through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  // Verify that the caret highlight is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Manually enable the caret highlight.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsCaretHighlightEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kCaretHighlightEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  // Verify that the caret highlight cannot be enabled manually anymore.
  accessibility_manager->SetCaretHighlightEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsCaretHighlightEnabled());

  policies.Set(key::kCaretHighlightEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(accessibility_manager->IsCaretHighlightEnabled());

  // Verify that the caret highlight cannot be disabled manually anymore.
  accessibility_manager->SetCaretHighlightEnabled(false);
  EXPECT_TRUE(accessibility_manager->IsCaretHighlightEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, MonoAudioEnabled) {
  // Verifies that the mono audio accessibility feature can be controlled
  // through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  accessibility_manager->EnableMonoAudio(false);
  // Verify that the mono audio is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Manually enable the mono audio.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_TRUE(accessibility_manager->IsMonoAudioEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kMonoAudioEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  // Verify that the mono audio cannot be enabled manually anymore.
  accessibility_manager->EnableMonoAudio(true);
  EXPECT_FALSE(accessibility_manager->IsMonoAudioEnabled());

  policies.Set(key::kMonoAudioEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(accessibility_manager->IsMonoAudioEnabled());

  // Verify that the mono audio cannot be disabled manually anymore.
  accessibility_manager->EnableMonoAudio(false);
  EXPECT_TRUE(accessibility_manager->IsMonoAudioEnabled());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, AutoclickEnabled) {
  // Verifies that the autoclick accessibility feature can be controlled through
  // policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  accessibility_manager->EnableAutoclick(false);
  // Verify that the autoclick is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Manually enable the autoclick.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kAutoclickEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  // Verify that the autoclick cannot be enabled manually anymore.
  accessibility_manager->EnableAutoclick(true);
  EXPECT_FALSE(accessibility_manager->IsAutoclickEnabled());

  policies.Set(key::kAutoclickEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that the autoclick cannot be disabled manually anymore.
  accessibility_manager->EnableAutoclick(false);
  EXPECT_TRUE(accessibility_manager->IsAutoclickEnabled());

  // Verify that no confirmation dialog has been shown.
  EXPECT_FALSE(accessibility_manager->IsDisableAutoclickDialogVisibleForTest());
}

IN_PROC_BROWSER_TEST_F(AccessibilityPolicyTest, ColorCorrectionEnabled) {
  // Verifies that the color correction accessibility feature can be controlled
  // through policy.
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();

  accessibility_manager->SetColorCorrectionEnabled(false);
  // Verify that color correction is initially disabled.
  EXPECT_FALSE(accessibility_manager->IsColorCorrectionEnabled());

  // Manually enable color correction.
  accessibility_manager->SetColorCorrectionEnabled(true);
  EXPECT_TRUE(accessibility_manager->IsColorCorrectionEnabled());

  // Verify that policy overrides the manual setting.
  PolicyMap policies;
  policies.Set(key::kColorCorrectionEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_FALSE(accessibility_manager->IsColorCorrectionEnabled());

  // Verify that color correction cannot be enabled manually anymore.
  accessibility_manager->SetColorCorrectionEnabled(true);
  EXPECT_FALSE(accessibility_manager->IsColorCorrectionEnabled());

  policies.Set(key::kColorCorrectionEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  EXPECT_TRUE(accessibility_manager->IsColorCorrectionEnabled());

  // Verify that color correction cannot be disabled manually anymore.
  accessibility_manager->SetColorCorrectionEnabled(false);
  EXPECT_TRUE(accessibility_manager->IsColorCorrectionEnabled());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
// Tests that the UiAutomationProviderEnabled policy is respected when set, and
// that the UiaProvider feature takes effect only when the policy is not set.
class UiAutomationProviderPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<
          std::tuple<PolicyTest::BooleanPolicy, bool>> {
 protected:
  static PolicyTest::BooleanPolicy GetBooleanPolicyParam() {
    return std::get<0>(GetParam());
  }

  static bool GetFeatureEnabledParam() { return std::get<1>(GetParam()); }

  UiAutomationProviderPolicyTest() {
    feature_list_.InitWithFeatureState(::features::kUiaProvider,
                                       GetFeatureEnabledParam());
  }

  void SetUpInProcessBrowserTestFixture() override {
    PolicyTest::SetUpInProcessBrowserTestFixture();
    if (const auto boolean_policy = GetBooleanPolicyParam();
        boolean_policy != BooleanPolicy::kNotConfigured) {
      PolicyMap policy_map;
      SetPolicy(&policy_map, key::kUiAutomationProviderEnabled,
                base::Value(boolean_policy == BooleanPolicy::kTrue));
      UpdateProviderPolicy(policy_map);
    }
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(UiAutomationProviderPolicyTest, IsUiaProviderEnabled) {
  if (const auto boolean_policy = GetBooleanPolicyParam();
      boolean_policy == BooleanPolicy::kNotConfigured) {
    // Enabled or disabled according to the variations framework.
    ASSERT_EQ(::ui::AXPlatform::GetInstance().IsUiaProviderEnabled(),
              GetFeatureEnabledParam());
  } else {
    // Enabled or disabled according to the value of the policy.
    ASSERT_EQ(::ui::AXPlatform::GetInstance().IsUiaProviderEnabled(),
              boolean_policy == BooleanPolicy::kTrue);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    UiAutomationProviderPolicyTest,
    ::testing::Combine(
        ::testing::Values(PolicyTest::BooleanPolicy::kNotConfigured,
                          PolicyTest::BooleanPolicy::kFalse,
                          PolicyTest::BooleanPolicy::kTrue),
        ::testing::Bool()));

#endif  // BUILDFLAG(IS_WIN)

}  // namespace policy
