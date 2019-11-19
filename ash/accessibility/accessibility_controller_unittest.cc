// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"

#include <utility>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

class TestAccessibilityObserver : public AccessibilityObserver {
 public:
  TestAccessibilityObserver() = default;
  ~TestAccessibilityObserver() override = default;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override { ++status_changed_count_; }

  int status_changed_count_ = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAccessibilityObserver);
};

using AccessibilityControllerTest = AshTestBase;

TEST_F(AccessibilityControllerTest, PrefsAreRegistered) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityAutoclickDelayMs));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorDipSize));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityScreenMagnifierScale));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
}

TEST_F(AccessibilityControllerTest, SetAutoclickEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->autoclick_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetAutoclickEnabled(true);
  EXPECT_TRUE(controller->autoclick_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetAutoclickEnabled(false);
  EXPECT_FALSE(controller->autoclick_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetCaretHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->caret_highlight_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetCaretHighlightEnabled(true);
  EXPECT_TRUE(controller->caret_highlight_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(controller->caret_highlight_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetCursorHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->cursor_highlight_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetCursorHighlightEnabled(true);
  EXPECT_TRUE(controller->cursor_highlight_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(controller->cursor_highlight_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetFocusHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->focus_highlight_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetFocusHighlightEnabled(true);
  EXPECT_TRUE(controller->focus_highlight_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetFocusHighlightEnabled(false);
  EXPECT_FALSE(controller->focus_highlight_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetHighContrastEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->high_contrast_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetHighContrastEnabled(true);
  EXPECT_TRUE(controller->high_contrast_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetHighContrastEnabled(false);
  EXPECT_FALSE(controller->high_contrast_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetLargeCursorEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->large_cursor_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetLargeCursorEnabled(true);
  EXPECT_TRUE(controller->large_cursor_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetLargeCursorEnabled(false);
  EXPECT_FALSE(controller->large_cursor_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, LargeCursorTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetLargeCursorEnabled(true);
  EXPECT_TRUE(controller->large_cursor_enabled());
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(controller->IsLargeCursorSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetLargeCursorEnabled(false);
  EXPECT_FALSE(controller->large_cursor_enabled());
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(controller->IsLargeCursorSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityLargeCursorEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(controller->large_cursor_enabled());
  EXPECT_TRUE(controller->IsLargeCursorSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityLargeCursorEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_FALSE(controller->large_cursor_enabled());
  EXPECT_FALSE(controller->IsLargeCursorSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, HighContrastTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetHighContrastEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(controller->high_contrast_enabled());
  EXPECT_TRUE(controller->IsHighContrastSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetHighContrastEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_FALSE(controller->high_contrast_enabled());
  EXPECT_TRUE(controller->IsHighContrastSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityHighContrastEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(controller->IsHighContrastSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityHighContrastEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_FALSE(controller->high_contrast_enabled());
  EXPECT_FALSE(controller->IsHighContrastSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, MonoAudioTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetMonoAudioEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(controller->mono_audio_enabled());
  EXPECT_TRUE(controller->IsMonoAudioSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetMonoAudioEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_FALSE(controller->mono_audio_enabled());
  EXPECT_TRUE(controller->IsMonoAudioSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityMonoAudioEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(controller->IsMonoAudioSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityMonoAudioEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_FALSE(controller->mono_audio_enabled());
  EXPECT_FALSE(controller->IsMonoAudioSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, DictationTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Required to set the dialog to be true to change the value of the pref from
  // the |AccessibilityControllerImpl|.
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  // Check when the value is true and not being controlled by any policy.
  controller->SetDictationEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_TRUE(controller->dictation_enabled());
  EXPECT_TRUE(controller->IsDictationSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetDictationEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_FALSE(controller->dictation_enabled());
  EXPECT_TRUE(controller->IsDictationSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityDictationEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_TRUE(controller->IsDictationSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityDictationEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_FALSE(controller->dictation_enabled());
  EXPECT_FALSE(controller->IsDictationSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, CursorHighlightTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetCursorHighlightEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_TRUE(controller->cursor_highlight_enabled());
  EXPECT_TRUE(controller->IsCursorHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetCursorHighlightEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_FALSE(controller->cursor_highlight_enabled());
  EXPECT_TRUE(controller->IsCursorHighlightSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityCursorHighlightEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_TRUE(controller->IsCursorHighlightSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityCursorHighlightEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_FALSE(controller->cursor_highlight_enabled());
  EXPECT_FALSE(controller->IsCursorHighlightSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, FullScreenMagnifierTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetFullscreenMagnifierEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(controller->IsFullscreenMagnifierEnabledForTesting());
  EXPECT_TRUE(controller->IsFullScreenMagnifierSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetFullscreenMagnifierEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_FALSE(controller->IsFullscreenMagnifierEnabledForTesting());
  EXPECT_TRUE(controller->IsFullScreenMagnifierSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityScreenMagnifierEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(controller->IsFullScreenMagnifierSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityScreenMagnifierEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_FALSE(controller->IsFullscreenMagnifierEnabledForTesting());
  EXPECT_FALSE(controller->IsFullScreenMagnifierSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, DockedMagnifierTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetDockedMagnifierEnabledForTesting(true);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_TRUE(controller->IsDockedMagnifierEnabledForTesting());
  EXPECT_TRUE(controller->IsDockedMagnifierSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetDockedMagnifierEnabledForTesting(false);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_FALSE(controller->IsDockedMagnifierEnabledForTesting());
  EXPECT_TRUE(controller->IsDockedMagnifierSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kDockedMagnifierEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_TRUE(controller->IsDockedMagnifierSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kDockedMagnifierEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_FALSE(controller->IsDockedMagnifierEnabledForTesting());
  EXPECT_FALSE(controller->IsDockedMagnifierSettingVisibleInTray());
}
TEST_F(AccessibilityControllerTest, CaretHighlightTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetCaretHighlightEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_TRUE(controller->caret_highlight_enabled());
  EXPECT_TRUE(controller->IsCaretHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetCaretHighlightEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_FALSE(controller->caret_highlight_enabled());
  EXPECT_TRUE(controller->IsCaretHighlightSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityCaretHighlightEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_TRUE(controller->IsCaretHighlightSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityCaretHighlightEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_FALSE(controller->caret_highlight_enabled());
  EXPECT_FALSE(controller->IsCaretHighlightSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, SelectToSpeakTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetSelectToSpeakEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_TRUE(controller->select_to_speak_enabled());
  EXPECT_TRUE(controller->IsSelectToSpeakSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetSelectToSpeakEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_FALSE(controller->select_to_speak_enabled());
  EXPECT_TRUE(controller->IsSelectToSpeakSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySelectToSpeakEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_TRUE(controller->IsSelectToSpeakSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySelectToSpeakEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_FALSE(controller->select_to_speak_enabled());
  EXPECT_FALSE(controller->IsSelectToSpeakSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, AutoClickTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetAutoclickEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(controller->autoclick_enabled());
  EXPECT_TRUE(controller->IsAutoclickSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetAutoclickEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_FALSE(controller->autoclick_enabled());
  EXPECT_TRUE(controller->IsAutoclickSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityAutoclickEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(controller->IsAutoclickSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityAutoclickEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_FALSE(controller->autoclick_enabled());
  EXPECT_FALSE(controller->IsAutoclickSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, SpokenFeedbackTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_TRUE(controller->spoken_feedback_enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_FALSE(controller->spoken_feedback_enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySpokenFeedbackEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_FALSE(controller->spoken_feedback_enabled());
  EXPECT_FALSE(controller->IsSpokenFeedbackSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, VirtualKeyboardTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetVirtualKeyboardEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_TRUE(controller->virtual_keyboard_enabled());
  EXPECT_TRUE(controller->IsVirtualKeyboardSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetVirtualKeyboardEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_FALSE(controller->virtual_keyboard_enabled());
  EXPECT_TRUE(controller->IsVirtualKeyboardSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_TRUE(controller->IsVirtualKeyboardSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityVirtualKeyboardEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_FALSE(controller->virtual_keyboard_enabled());
  EXPECT_FALSE(controller->IsVirtualKeyboardSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, SwitchAccessTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetSwitchAccessEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_TRUE(controller->switch_access_enabled());
  EXPECT_TRUE(controller->IsSwitchAccessSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetSwitchAccessEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_FALSE(controller->switch_access_enabled());
  EXPECT_TRUE(controller->IsSwitchAccessSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySwitchAccessEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_TRUE(controller->IsSwitchAccessSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilitySwitchAccessEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_FALSE(controller->switch_access_enabled());
  EXPECT_FALSE(controller->IsSwitchAccessSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, FocusHighlightTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetFocusHighlightEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_TRUE(controller->focus_highlight_enabled());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetFocusHighlightEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_FALSE(controller->focus_highlight_enabled());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityFocusHighlightEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityFocusHighlightEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_FALSE(controller->focus_highlight_enabled());
  EXPECT_FALSE(controller->IsFocusHighlightSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, StickyKeysTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->SetStickyKeysEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_TRUE(controller->sticky_keys_enabled());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetStickyKeysEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_FALSE(controller->sticky_keys_enabled());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityStickyKeysEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityStickyKeysEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_FALSE(controller->sticky_keys_enabled());
  EXPECT_FALSE(controller->IsStickyKeysSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, DisableLargeCursorResetsSize) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  EXPECT_EQ(kDefaultLargeCursorSize,
            prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize));

  // Simulate using chrome settings webui to turn on large cursor and set a
  // custom size.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);
  prefs->SetInteger(prefs::kAccessibilityLargeCursorDipSize, 48);

  // Turning off large cursor resets the size to the default.
  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, false);
  EXPECT_EQ(kDefaultLargeCursorSize,
            prefs->GetInteger(prefs::kAccessibilityLargeCursorDipSize));
}

TEST_F(AccessibilityControllerTest, SetMonoAudioEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->mono_audio_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetMonoAudioEnabled(true);
  EXPECT_TRUE(controller->mono_audio_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetMonoAudioEnabled(false);
  EXPECT_FALSE(controller->mono_audio_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetSpokenFeedbackEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->spoken_feedback_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(controller->spoken_feedback_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetStickyKeysEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->sticky_keys_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  StickyKeysController* sticky_keys_controller =
      Shell::Get()->sticky_keys_controller();
  controller->SetStickyKeysEnabled(true);
  EXPECT_TRUE(sticky_keys_controller->enabled_for_test());
  EXPECT_TRUE(controller->sticky_keys_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetStickyKeysEnabled(false);
  EXPECT_FALSE(sticky_keys_controller->enabled_for_test());
  EXPECT_FALSE(controller->sticky_keys_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetVirtualKeyboardEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->virtual_keyboard_enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetVirtualKeyboardEnabled(true);
  EXPECT_TRUE(keyboard::GetAccessibilityKeyboardEnabled());
  EXPECT_TRUE(controller->virtual_keyboard_enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetVirtualKeyboardEnabled(false);
  EXPECT_FALSE(keyboard::GetAccessibilityKeyboardEnabled());
  EXPECT_FALSE(controller->virtual_keyboard_enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

// The controller should get ShutdownSoundDuration from its client.
TEST_F(AccessibilityControllerTest, GetShutdownSoundDuration) {
  TestAccessibilityControllerClient client;
  EXPECT_EQ(TestAccessibilityControllerClient::kShutdownSoundDuration,
            client.PlayShutdownSound());
  EXPECT_EQ(TestAccessibilityControllerClient::kShutdownSoundDuration,
            Shell::Get()->accessibility_controller()->PlayShutdownSound());
}

// The controller should get ShouldToggleSpokenFeedbackViaTouch from its client.
TEST_F(AccessibilityControllerTest, GetShouldToggleSpokenFeedbackViaTouch) {
  TestAccessibilityControllerClient client;
  EXPECT_TRUE(client.ShouldToggleSpokenFeedbackViaTouch());
  EXPECT_TRUE(Shell::Get()
                  ->accessibility_controller()
                  ->ShouldToggleSpokenFeedbackViaTouch());
}

TEST_F(AccessibilityControllerTest, SetDarkenScreen) {
  ASSERT_FALSE(
      chromeos::FakePowerManagerClient::Get()->backlights_forced_off());

  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  controller->SetDarkenScreen(true);
  EXPECT_TRUE(chromeos::FakePowerManagerClient::Get()->backlights_forced_off());

  controller->SetDarkenScreen(false);
  EXPECT_FALSE(
      chromeos::FakePowerManagerClient::Get()->backlights_forced_off());
}

TEST_F(AccessibilityControllerTest, ShowNotificationOnSpokenFeedback) {
  const base::string16 kChromeVoxEnabledTitle =
      base::ASCIIToUTF16("ChromeVox enabled");
  const base::string16 kChromeVoxEnabled =
      base::ASCIIToUTF16("Press Ctrl + Alt + Z to disable spoken feedback.");
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  // Enabling spoken feedback should show the notification if specified to show
  // notification.
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(kChromeVoxEnabledTitle, (*notifications.begin())->title());
  EXPECT_EQ(kChromeVoxEnabled, (*notifications.begin())->message());

  // Disabling spoken feedback should not show any notification even if
  // specified to show notification.
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());

  // Enabling spoken feedback but not specified to show notification should not
  // show any notification, for example toggling on tray detailed menu.
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
}

TEST_F(AccessibilityControllerTest,
       ShowNotificationOnBrailleDisplayStateChanged) {
  const base::string16 kBrailleConnected =
      base::ASCIIToUTF16("Braille display connected.");
  const base::string16 kChromeVoxEnabled =
      base::ASCIIToUTF16("Press Ctrl + Alt + Z to disable spoken feedback.");
  const base::string16 kBrailleConnectedAndChromeVoxEnabledTitle =
      base::ASCIIToUTF16("Braille and ChromeVox are enabled");
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->spoken_feedback_enabled());
  // Connecting a braille display when spoken feedback is already enabled
  // should only show the message about the braille display.
  controller->BrailleDisplayStateChanged(true);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(base::string16(), (*notifications.begin())->title());
  EXPECT_EQ(kBrailleConnected, (*notifications.begin())->message());

  // Neither disconnecting a braille display, nor disabling spoken feedback
  // should show any notification.
  controller->BrailleDisplayStateChanged(false);
  EXPECT_TRUE(controller->spoken_feedback_enabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  EXPECT_FALSE(controller->spoken_feedback_enabled());

  // Connecting a braille display should enable spoken feedback and show
  // both messages.
  controller->BrailleDisplayStateChanged(true);
  EXPECT_TRUE(controller->spoken_feedback_enabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(kBrailleConnectedAndChromeVoxEnabledTitle,
            (*notifications.begin())->title());
  EXPECT_EQ(kChromeVoxEnabled, (*notifications.begin())->message());
}

TEST_F(AccessibilityControllerTest, SelectToSpeakStateChanges) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);

  controller->SetSelectToSpeakState(
      ash::SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_EQ(controller->GetSelectToSpeakState(),
            ash::SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_EQ(observer.status_changed_count_, 1);

  controller->SetSelectToSpeakState(
      ash::SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_EQ(controller->GetSelectToSpeakState(),
            ash::SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_EQ(observer.status_changed_count_, 2);

  controller->RemoveObserver(&observer);
}

namespace {

enum class TestUserLoginType {
  kNewUser,
  kGuest,
  kExistingUser,
};

class AccessibilityControllerSigninTest
    : public NoSessionAshTestBase,
      public testing::WithParamInterface<TestUserLoginType> {
 public:
  AccessibilityControllerSigninTest() = default;
  ~AccessibilityControllerSigninTest() = default;

  void SimulateLogin() {
    constexpr char kUserEmail[] = "user1@test.com";
    switch (GetParam()) {
      case TestUserLoginType::kNewUser:
        SimulateNewUserFirstLogin(kUserEmail);
        break;

      case TestUserLoginType::kGuest:
        SimulateGuestLogin();
        break;

      case TestUserLoginType::kExistingUser:
        SimulateUserLogin(kUserEmail);
        break;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AccessibilityControllerSigninTest);
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(,
                         AccessibilityControllerSigninTest,
                         ::testing::Values(TestUserLoginType::kNewUser,
                                           TestUserLoginType::kGuest,
                                           TestUserLoginType::kExistingUser));

TEST_P(AccessibilityControllerSigninTest, EnableOnLoginScreenAndLogin) {
  constexpr float kMagnifierScale = 4.3f;

  AccessibilityControllerImpl* accessibility =
      Shell::Get()->accessibility_controller();
  DockedMagnifierControllerImpl* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();

  SessionControllerImpl* session = Shell::Get()->session_controller();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session->GetSessionState());
  EXPECT_FALSE(accessibility->large_cursor_enabled());
  EXPECT_FALSE(accessibility->spoken_feedback_enabled());
  EXPECT_FALSE(accessibility->high_contrast_enabled());
  EXPECT_FALSE(accessibility->autoclick_enabled());
  EXPECT_FALSE(accessibility->mono_audio_enabled());
  EXPECT_FALSE(docked_magnifier->GetEnabled());
  using prefs::kAccessibilityLargeCursorEnabled;
  using prefs::kAccessibilitySpokenFeedbackEnabled;
  using prefs::kAccessibilityHighContrastEnabled;
  using prefs::kAccessibilityAutoclickEnabled;
  using prefs::kAccessibilityMonoAudioEnabled;
  using prefs::kDockedMagnifierEnabled;
  PrefService* signin_prefs = session->GetSigninScreenPrefService();
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kDockedMagnifierEnabled));

  // Verify that toggling prefs at the signin screen changes the signin setting.
  accessibility->SetLargeCursorEnabled(true);
  accessibility->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  accessibility->SetHighContrastEnabled(true);
  accessibility->SetAutoclickEnabled(true);
  accessibility->SetMonoAudioEnabled(true);
  docked_magnifier->SetEnabled(true);
  docked_magnifier->SetScale(kMagnifierScale);
  // TODO(afakhry): Test the Fullscreen magnifier prefs once the
  // ash::MagnificationController handles all the prefs work itself inside ash
  // without needing magnification manager in Chrome.
  EXPECT_TRUE(accessibility->large_cursor_enabled());
  EXPECT_TRUE(accessibility->spoken_feedback_enabled());
  EXPECT_TRUE(accessibility->high_contrast_enabled());
  EXPECT_TRUE(accessibility->autoclick_enabled());
  EXPECT_TRUE(accessibility->mono_audio_enabled());
  EXPECT_TRUE(docked_magnifier->GetEnabled());
  EXPECT_FLOAT_EQ(kMagnifierScale, docked_magnifier->GetScale());
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kDockedMagnifierEnabled));

  SimulateLogin();

  // Verify that prefs values are copied if they should.
  PrefService* user_prefs = session->GetLastActiveUserPrefService();
  EXPECT_NE(signin_prefs, user_prefs);
  const bool should_signin_prefs_be_copied =
      GetParam() == TestUserLoginType::kNewUser ||
      GetParam() == TestUserLoginType::kGuest;
  if (should_signin_prefs_be_copied) {
    EXPECT_TRUE(accessibility->large_cursor_enabled());
    EXPECT_TRUE(accessibility->spoken_feedback_enabled());
    EXPECT_TRUE(accessibility->high_contrast_enabled());
    EXPECT_TRUE(accessibility->autoclick_enabled());
    EXPECT_TRUE(accessibility->mono_audio_enabled());
    EXPECT_TRUE(docked_magnifier->GetEnabled());
    EXPECT_FLOAT_EQ(kMagnifierScale, docked_magnifier->GetScale());
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kDockedMagnifierEnabled));
  } else {
    EXPECT_FALSE(accessibility->large_cursor_enabled());
    EXPECT_FALSE(accessibility->spoken_feedback_enabled());
    EXPECT_FALSE(accessibility->high_contrast_enabled());
    EXPECT_FALSE(accessibility->autoclick_enabled());
    EXPECT_FALSE(accessibility->mono_audio_enabled());
    EXPECT_FALSE(docked_magnifier->GetEnabled());
    EXPECT_NE(kMagnifierScale, docked_magnifier->GetScale());
    EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
    EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
    EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
    EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
    EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
    EXPECT_FALSE(user_prefs->GetBoolean(kDockedMagnifierEnabled));
  }
}

}  // namespace ash
