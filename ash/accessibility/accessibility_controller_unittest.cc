// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_controller_impl.h"

#include <string>
#include <utility>

#include "ash/accessibility/a11y_feature_type.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/test/test_system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_pref_service_provider.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/aura/aura_window_properties.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

using message_center::MessageCenter;

namespace ash {

class TestAccessibilityObserver : public AccessibilityObserver {
 public:
  TestAccessibilityObserver() = default;
  TestAccessibilityObserver(const TestAccessibilityObserver&) = delete;
  TestAccessibilityObserver& operator=(const TestAccessibilityObserver&) =
      delete;
  ~TestAccessibilityObserver() override = default;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override { ++status_changed_count_; }

  int status_changed_count_ = 0;
};

class AccessibilityControllerTest : public AshTestBase {
 protected:
  AccessibilityControllerTest() = default;
  AccessibilityControllerTest(const AccessibilityControllerTest&) = delete;
  AccessibilityControllerTest& operator=(const AccessibilityControllerTest&) =
      delete;
  ~AccessibilityControllerTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
         ash::features::kOnDeviceSpeechRecognition},
        {});
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AccessibilityControllerTest, ChangingCursorSizePrefChangesCursorSize) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  prefs->SetBoolean(prefs::kAccessibilityLargeCursorEnabled, true);

  CursorWindowController* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();

  // Test all possible sizes
  for (int size = 25; size <= 64; ++size) {
    prefs->SetInteger(prefs::kAccessibilityLargeCursorDipSize, size);
    auto bounds = cursor_window_controller->GetBoundsForTest();
    EXPECT_EQ(bounds.height(), size);
    EXPECT_EQ(bounds.width(), size);
  }
}

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
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityDictationLocale));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityLargeCursorDipSize));
  EXPECT_TRUE(prefs->FindPreference(::prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityScreenMagnifierFocusFollowingEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityScreenMagnifierScale));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityChromeVoxAutoRead));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxAnnounceDownloadNotifications));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxAnnounceRichTextAttributes));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxAudioStrategy));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleSideBySide));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleTable));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleTable6));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleTable8));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleTableType));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxBrailleWordWrap));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxCapitalStrategy));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxCapitalStrategyBackup));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxEnableBrailleLogging));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxEnableEarconLogging));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxEnableEventStreamLogging));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxEnableSpeechLogging));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxEventStreamFilters));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxLanguageSwitching));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxMenuBrailleCommands));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxNumberReadingStyle));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxPreferredBrailleDisplayAddress));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxPunctuationEcho));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxSmartStickyMode));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxSpeakTextUnderMouse));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxUsePitchChanges));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxUseVerboseMode));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityChromeVoxVirtualBrailleColumns));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityChromeVoxVirtualBrailleRows));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityChromeVoxVoiceName));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_TRUE(prefs->FindPreference(prefs::kAccessibilityShortcutsEnabled));
  EXPECT_TRUE(
      prefs->FindPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_TRUE(prefs->FindPreference(
      prefs::kAccessibilityEnhancedNetworkVoicesInSelectToSpeakAllowed));
    EXPECT_TRUE(
        prefs->FindPreference(prefs::kAccessibilityColorCorrectionEnabled));
    EXPECT_TRUE(prefs->FindPreference(
        prefs::kAccessibilityColorCorrectionHasBeenSetup));
    EXPECT_TRUE(
        prefs->FindPreference(prefs::kAccessibilityColorVisionCorrectionType));
    EXPECT_TRUE(prefs->FindPreference(
        prefs::kAccessibilityColorVisionCorrectionAmount));
}

TEST_F(AccessibilityControllerTest, SetAutoclickEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->autoclick().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->autoclick().SetEnabled(true);
  EXPECT_TRUE(controller->autoclick().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->autoclick().SetEnabled(false);
  EXPECT_FALSE(controller->autoclick().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetCaretHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->caret_highlight().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->caret_highlight().SetEnabled(true);
  EXPECT_TRUE(controller->caret_highlight().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->caret_highlight().SetEnabled(false);
  EXPECT_FALSE(controller->caret_highlight().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetColorCorrectionEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->color_correction().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  EXPECT_EQ(0, GetSystemTrayClient()->show_color_correction_settings_count());

  controller->color_correction().SetEnabled(true);
  EXPECT_TRUE(controller->color_correction().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  // The first time we should show the settings for color correction.
  EXPECT_EQ(1, GetSystemTrayClient()->show_color_correction_settings_count());

  controller->color_correction().SetEnabled(false);
  EXPECT_FALSE(controller->color_correction().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->color_correction().SetEnabled(true);
  EXPECT_TRUE(controller->color_correction().enabled());
  EXPECT_EQ(3, observer.status_changed_count_);

  // The second time, the settings window should not be opened.
  EXPECT_EQ(1, GetSystemTrayClient()->show_color_correction_settings_count());

  controller->color_correction().SetEnabled(false);
  EXPECT_FALSE(controller->color_correction().enabled());
  EXPECT_EQ(4, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetCursorHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->cursor_highlight().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->cursor_highlight().SetEnabled(true);
  EXPECT_TRUE(controller->cursor_highlight().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->cursor_highlight().SetEnabled(false);
  EXPECT_FALSE(controller->cursor_highlight().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetFocusHighlightEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->focus_highlight().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->focus_highlight().SetEnabled(true);
  EXPECT_TRUE(controller->focus_highlight().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->focus_highlight().SetEnabled(false);
  EXPECT_FALSE(controller->focus_highlight().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetHighContrastEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->high_contrast().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->high_contrast().SetEnabled(true);
  EXPECT_TRUE(controller->high_contrast().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->high_contrast().SetEnabled(false);
  EXPECT_FALSE(controller->high_contrast().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetLargeCursorEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->large_cursor().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->large_cursor().SetEnabled(true);
  EXPECT_TRUE(controller->large_cursor().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->large_cursor().SetEnabled(false);
  EXPECT_FALSE(controller->large_cursor().enabled());
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
  controller->large_cursor().SetEnabled(true);
  EXPECT_TRUE(controller->large_cursor().enabled());
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_TRUE(controller->IsLargeCursorSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->large_cursor().SetEnabled(false);
  EXPECT_FALSE(controller->large_cursor().enabled());
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
  EXPECT_TRUE(controller->large_cursor().enabled());
  EXPECT_TRUE(controller->IsLargeCursorSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityLargeCursorEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityLargeCursorEnabled));
  EXPECT_FALSE(controller->large_cursor().enabled());
  EXPECT_FALSE(controller->IsLargeCursorSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, SetLiveCaptionEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->live_caption().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, LiveCaptionTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->live_caption().SetEnabled(true);
  EXPECT_TRUE(controller->live_caption().enabled());
  EXPECT_FALSE(prefs->IsManagedPreference(::prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(controller->IsLiveCaptionSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->live_caption().SetEnabled(false);
  EXPECT_FALSE(controller->live_caption().enabled());
  EXPECT_FALSE(prefs->IsManagedPreference(::prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(controller->IsLiveCaptionSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      ::prefs::kLiveCaptionEnabled, std::make_unique<base::Value>(true));
  EXPECT_TRUE(prefs->IsManagedPreference(::prefs::kLiveCaptionEnabled));
  EXPECT_TRUE(controller->live_caption().enabled());
  EXPECT_TRUE(controller->IsLiveCaptionSettingVisibleInTray());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      ::prefs::kLiveCaptionEnabled, std::make_unique<base::Value>(false));
  EXPECT_TRUE(prefs->IsManagedPreference(::prefs::kLiveCaptionEnabled));
  EXPECT_FALSE(controller->live_caption().enabled());
  EXPECT_FALSE(controller->IsLiveCaptionSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, HighContrastTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->high_contrast().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_TRUE(controller->high_contrast().enabled());
  EXPECT_TRUE(controller->IsHighContrastSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->high_contrast().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityHighContrastEnabled));
  EXPECT_FALSE(controller->high_contrast().enabled());
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
  EXPECT_FALSE(controller->high_contrast().enabled());
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
  controller->mono_audio().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_TRUE(controller->mono_audio().enabled());
  EXPECT_TRUE(controller->IsMonoAudioSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->mono_audio().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityMonoAudioEnabled));
  EXPECT_FALSE(controller->mono_audio().enabled());
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
  EXPECT_FALSE(controller->mono_audio().enabled());
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
  controller->dictation().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_TRUE(controller->dictation().enabled());
  EXPECT_TRUE(controller->IsDictationSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->dictation().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityDictationEnabled));
  EXPECT_FALSE(controller->dictation().enabled());
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
  EXPECT_FALSE(controller->dictation().enabled());
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
  controller->cursor_highlight().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_TRUE(controller->cursor_highlight().enabled());
  EXPECT_TRUE(controller->IsCursorHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->cursor_highlight().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCursorHighlightEnabled));
  EXPECT_FALSE(controller->cursor_highlight().enabled());
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
  EXPECT_FALSE(controller->cursor_highlight().enabled());
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
  controller->fullscreen_magnifier().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_TRUE(controller->fullscreen_magnifier().enabled());
  EXPECT_TRUE(controller->IsFullScreenMagnifierSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->fullscreen_magnifier().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityScreenMagnifierEnabled));
  EXPECT_FALSE(controller->fullscreen_magnifier().enabled());
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
  EXPECT_FALSE(controller->fullscreen_magnifier().enabled());
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
  controller->docked_magnifier().SetEnabled(true);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_TRUE(controller->docked_magnifier().enabled());
  EXPECT_TRUE(controller->IsDockedMagnifierSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->docked_magnifier().SetEnabled(false);
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kDockedMagnifierEnabled));
  EXPECT_FALSE(controller->docked_magnifier().enabled());
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
  EXPECT_FALSE(controller->docked_magnifier().enabled());
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
  controller->caret_highlight().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_TRUE(controller->caret_highlight().enabled());
  EXPECT_TRUE(controller->IsCaretHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->caret_highlight().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityCaretHighlightEnabled));
  EXPECT_FALSE(controller->caret_highlight().enabled());
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
  EXPECT_FALSE(controller->caret_highlight().enabled());
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
  controller->select_to_speak().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_TRUE(controller->select_to_speak().enabled());
  EXPECT_TRUE(controller->IsSelectToSpeakSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->select_to_speak().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySelectToSpeakEnabled));
  EXPECT_FALSE(controller->select_to_speak().enabled());
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
  EXPECT_FALSE(controller->select_to_speak().enabled());
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
  controller->autoclick().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_TRUE(controller->autoclick().enabled());
  EXPECT_TRUE(controller->IsAutoclickSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->autoclick().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityAutoclickEnabled));
  EXPECT_FALSE(controller->autoclick().enabled());
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
  EXPECT_FALSE(controller->autoclick().enabled());
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
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySpokenFeedbackEnabled));
  EXPECT_FALSE(controller->spoken_feedback().enabled());
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
  EXPECT_FALSE(controller->spoken_feedback().enabled());
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
  controller->virtual_keyboard().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_TRUE(controller->virtual_keyboard().enabled());
  EXPECT_TRUE(controller->IsVirtualKeyboardSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->virtual_keyboard().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityVirtualKeyboardEnabled));
  EXPECT_FALSE(controller->virtual_keyboard().enabled());
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
  EXPECT_FALSE(controller->virtual_keyboard().enabled());
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
  controller->switch_access().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_TRUE(controller->switch_access().enabled());
  EXPECT_TRUE(controller->IsSwitchAccessSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->switch_access().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilitySwitchAccessEnabled));
  EXPECT_FALSE(controller->switch_access().enabled());
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
  EXPECT_FALSE(controller->switch_access().enabled());
  EXPECT_FALSE(controller->IsSwitchAccessSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, ColorCorrectionTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->color_correction().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityColorCorrectionEnabled));
  EXPECT_TRUE(controller->color_correction().enabled());
  EXPECT_TRUE(controller->IsColorCorrectionSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->color_correction().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityColorCorrectionEnabled));
  EXPECT_FALSE(controller->color_correction().enabled());
  EXPECT_TRUE(controller->IsColorCorrectionSettingVisibleInTray());

  // Check that when the pref is managed and being forced on then it will be
  // visible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityColorCorrectionEnabled,
      std::make_unique<base::Value>(true));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityColorCorrectionEnabled));
  EXPECT_TRUE(controller->IsColorCorrectionSettingVisibleInTray());
  EXPECT_TRUE(controller->color_correction().enabled());
  // Check that when the pref is managed and only being forced off then it will
  // be invisible.
  static_cast<TestingPrefServiceSimple*>(prefs)->SetManagedPref(
      prefs::kAccessibilityColorCorrectionEnabled,
      std::make_unique<base::Value>(false));
  EXPECT_TRUE(
      prefs->IsManagedPreference(prefs::kAccessibilityColorCorrectionEnabled));
  EXPECT_FALSE(controller->color_correction().enabled());
  EXPECT_FALSE(controller->IsColorCorrectionSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, FocusHighlightTrayMenuVisibility) {
  // Check that when the pref isn't being controlled by any policy will be
  // visible in the accessibility tray menu despite its value.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  // Check when the value is true and not being controlled by any policy.
  controller->focus_highlight().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_TRUE(controller->focus_highlight().enabled());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->focus_highlight().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityFocusHighlightEnabled));
  EXPECT_FALSE(controller->focus_highlight().enabled());
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
  EXPECT_FALSE(controller->focus_highlight().enabled());
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
  controller->sticky_keys().SetEnabled(true);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_TRUE(controller->sticky_keys().enabled());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());
  // Check when the value is false and not being controlled by any policy.
  controller->sticky_keys().SetEnabled(false);
  EXPECT_FALSE(
      prefs->IsManagedPreference(prefs::kAccessibilityStickyKeysEnabled));
  EXPECT_FALSE(controller->sticky_keys().enabled());
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
  EXPECT_FALSE(controller->sticky_keys().enabled());
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

TEST_F(AccessibilityControllerTest, ChangingCursorColorPrefChangesCursorColor) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Simulate using chrome settings webui to set cursor color, which also turns
  // on the cursor color enabled pref.
  prefs->SetInteger(prefs::kAccessibilityCursorColor, SK_ColorBLUE);
  prefs->SetBoolean(prefs::kAccessibilityCursorColorEnabled, true);

  CursorWindowController* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();

  // Expect cursor color in cursor_window_controller to be blue.
  EXPECT_EQ(SK_ColorBLUE, cursor_window_controller->GetCursorColorForTest());

  // Set cursor color pref to green.
  prefs->SetInteger(prefs::kAccessibilityCursorColor, SK_ColorGREEN);

  // Expect cursor color in cursor_window_controller to be green.
  EXPECT_EQ(SK_ColorGREEN, cursor_window_controller->GetCursorColorForTest());

  // Simulate using chrome settings webui to set cursor color to black, which
  // which also turns off the cursor color enabled pref.
  prefs->SetInteger(prefs::kAccessibilityCursorColor, 0);
  prefs->SetBoolean(prefs::kAccessibilityCursorColorEnabled, false);
  EXPECT_EQ(kDefaultCursorColor,
            cursor_window_controller->GetCursorColorForTest());
}

TEST_F(AccessibilityControllerTest, SetMonoAudioEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->mono_audio().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->mono_audio().SetEnabled(true);
  EXPECT_TRUE(controller->mono_audio().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->mono_audio().SetEnabled(false);
  EXPECT_FALSE(controller->mono_audio().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetSpokenFeedbackEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, FeaturesConflictingWithChromeVox) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  EXPECT_FALSE(controller->sticky_keys().enabled());
  EXPECT_FALSE(controller->focus_highlight().enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());

  controller->sticky_keys().SetEnabled(true);
  controller->focus_highlight().SetEnabled(true);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  EXPECT_TRUE(controller->sticky_keys().enabled());
  EXPECT_TRUE(controller->focus_highlight().enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());

  // Turning on Spoken Feedback will make sticky keys, focus highlight
  // act disabled and disappear from the tray.
  controller->spoken_feedback().SetEnabled(true);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  EXPECT_FALSE(controller->sticky_keys().enabled());
  EXPECT_FALSE(controller->focus_highlight().enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  EXPECT_FALSE(controller->IsFocusHighlightSettingVisibleInTray());
  EXPECT_FALSE(controller->IsStickyKeysSettingVisibleInTray());

  // Disabling ChromeVox will reset to previous state.
  controller->spoken_feedback().SetEnabled(false);
  EXPECT_FALSE(controller->spoken_feedback().enabled());
  EXPECT_TRUE(controller->sticky_keys().enabled());
  EXPECT_TRUE(controller->focus_highlight().enabled());
  EXPECT_TRUE(controller->IsSpokenFeedbackSettingVisibleInTray());
  EXPECT_TRUE(controller->IsFocusHighlightSettingVisibleInTray());
  EXPECT_TRUE(controller->IsStickyKeysSettingVisibleInTray());
}

TEST_F(AccessibilityControllerTest, SetStickyKeysEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->sticky_keys().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  StickyKeysController* sticky_keys_controller =
      Shell::Get()->sticky_keys_controller();
  controller->sticky_keys().SetEnabled(true);
  EXPECT_TRUE(sticky_keys_controller->enabled_for_test());
  EXPECT_TRUE(controller->sticky_keys().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->sticky_keys().SetEnabled(false);
  EXPECT_FALSE(sticky_keys_controller->enabled_for_test());
  EXPECT_FALSE(controller->sticky_keys().enabled());
  EXPECT_EQ(2, observer.status_changed_count_);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest, SetVirtualKeyboardEnabled) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  EXPECT_FALSE(controller->virtual_keyboard().enabled());

  TestAccessibilityObserver observer;
  controller->AddObserver(&observer);
  EXPECT_EQ(0, observer.status_changed_count_);

  controller->virtual_keyboard().SetEnabled(true);
  EXPECT_TRUE(keyboard::GetAccessibilityKeyboardEnabled());
  EXPECT_TRUE(controller->virtual_keyboard().enabled());
  EXPECT_EQ(1, observer.status_changed_count_);

  controller->virtual_keyboard().SetEnabled(false);
  EXPECT_FALSE(keyboard::GetAccessibilityKeyboardEnabled());
  EXPECT_FALSE(controller->virtual_keyboard().enabled());
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
  const std::u16string kChromeVoxEnabledTitle = u"ChromeVox enabled";
  const std::u16string kChromeVoxEnabled =
      u"Press Ctrl + Alt + Z to disable spoken feedback.";
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
  const std::u16string kBrailleConnected = u"Braille display connected.";
  const std::u16string kChromeVoxEnabled =
      u"Press Ctrl + Alt + Z to disable spoken feedback.";
  const std::u16string kBrailleConnectedAndChromeVoxEnabledTitle =
      u"Braille and ChromeVox are enabled";
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_SHOW);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  // Connecting a braille display when spoken feedback is already enabled
  // should only show the message about the braille display.
  controller->BrailleDisplayStateChanged(true);
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(std::u16string(), (*notifications.begin())->title());
  EXPECT_EQ(kBrailleConnected, (*notifications.begin())->message());

  // Neither disconnecting a braille display, nor disabling spoken feedback
  // should show any notification.
  controller->BrailleDisplayStateChanged(false);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_SHOW);
  notifications = MessageCenter::Get()->GetVisibleNotifications();
  EXPECT_EQ(0u, notifications.size());
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  // Connecting a braille display should enable spoken feedback and show
  // both messages.
  controller->BrailleDisplayStateChanged(true);
  EXPECT_TRUE(controller->spoken_feedback().enabled());
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
      SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_EQ(controller->GetSelectToSpeakState(),
            SelectToSpeakState::kSelectToSpeakStateSelecting);
  EXPECT_EQ(observer.status_changed_count_, 1);

  controller->SetSelectToSpeakState(
      SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_EQ(controller->GetSelectToSpeakState(),
            SelectToSpeakState::kSelectToSpeakStateSpeaking);
  EXPECT_EQ(observer.status_changed_count_, 2);

  controller->RemoveObserver(&observer);
}

TEST_F(AccessibilityControllerTest,
       SpeechRecognitionDownloadSucceededNotification) {
  const std::u16string kSucceededTitle =
      u"English speech files partially downloaded";
  const std::u16string kSucceededDescription =
      u"Speech is processed locally and dictation works offline, but some "
      u"voice commands won’t work.";
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  controller->ShowNotificationForDictation(
      DictationNotificationType::kOnlySodaDownloaded, u"English");
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(kSucceededTitle, (*notifications.begin())->title());
  EXPECT_EQ(kSucceededDescription, (*notifications.begin())->message());
  EXPECT_EQ(u"Dictation", (*notifications.begin())->display_source());
  EXPECT_EQ(message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
            (*notifications.begin())->system_notification_warning_level());
}

TEST_F(AccessibilityControllerTest,
       SpeechRecognitionDownloadFailedNotification) {
  const std::u16string kFailedTitle = u"Couldn't download English speech files";
  const std::u16string kFailedDescription =
      u"Download will be attempted later. Speech will be sent to Google for "
      u"processing for now.";
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();

  controller->ShowNotificationForDictation(
      DictationNotificationType::kNoDlcsDownloaded, u"English");
  message_center::NotificationList::Notifications notifications =
      MessageCenter::Get()->GetVisibleNotifications();
  ASSERT_EQ(1u, notifications.size());
  EXPECT_EQ(kFailedTitle, (*notifications.begin())->title());
  EXPECT_EQ(kFailedDescription, (*notifications.begin())->message());
  EXPECT_EQ(u"Dictation", (*notifications.begin())->display_source());
  EXPECT_EQ(message_center::SystemNotificationWarningLevel::CRITICAL_WARNING,
            (*notifications.begin())->system_notification_warning_level());
}

// Test to ensure all features that are toggleable in the quicksettings menu
// have a valid `name_resource_id`.
TEST_F(AccessibilityControllerTest, AllAccessibilityFeaturesHaveValidNames) {
  auto* accessibility_controller = Shell::Get()->accessibility_controller();
  for (int type = 0; type != static_cast<int>(A11yFeatureType::kFeatureCount);
       type++) {
    auto& feature = accessibility_controller->GetFeature(
        static_cast<A11yFeatureType>(type));
    if (!feature.toggleable_in_quicksettings()) {
      continue;
    }

    std::u16string feature_name =
        l10n_util::GetStringUTF16(feature.name_resource_id());
    EXPECT_GT(feature_name.length(), 0u);
  }
}

// Verifies the behavior of EnableOrToggleDictation without the keyboard
// improvements feature (current behavior).
TEST_F(AccessibilityControllerTest, EnableOrToggleDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->SetClient(&client);
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // If Dictation is disabled, then EnableOrToggleDictation should do nothing.
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());

  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());

  // If Dictation is enabled, then EnableOrToggleDictation should toggle
  // Dictation on/off.
  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  ASSERT_FALSE(controller->dictation_active());
  controller->dictation().SetEnabled(true);
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  ASSERT_TRUE(controller->dictation().enabled());
  ASSERT_TRUE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  ASSERT_TRUE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());
}

class AccessibilityControllerDictationKeyboardImprovementsTest
    : public AshTestBase {
 protected:
  AccessibilityControllerDictationKeyboardImprovementsTest() = default;
  AccessibilityControllerDictationKeyboardImprovementsTest(
      const AccessibilityControllerDictationKeyboardImprovementsTest&) = delete;
  AccessibilityControllerDictationKeyboardImprovementsTest& operator=(
      const AccessibilityControllerDictationKeyboardImprovementsTest&) = delete;
  ~AccessibilityControllerDictationKeyboardImprovementsTest() override =
      default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kAccessibilityDictationKeyboardImprovements);
    AshTestBase::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies the behavior of EnableOrToggleDictation with the keyboard
// improvements feature (new behavior).
TEST_F(AccessibilityControllerDictationKeyboardImprovementsTest,
       EnableOrToggleDictation) {
  AccessibilityControllerImpl* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;
  controller->SetClient(&client);
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  // Dictation disabled.

  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, false);
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  // If the dialog hasn't been accepted yet, then pressing the Dictation key
  // should show a dialog.
  ASSERT_FALSE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_TRUE(controller->IsDictationKeyboardDialogShowingForTesting());

  controller->DismissDictationKeyboardDialogForTesting();
  prefs->SetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted, true);
  controller->dictation().SetEnabled(false);
  controller->SetDictationActive(false);
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  // If the dialog has been accepted, then pressing the Dictation key should
  // enable Dictation (Dictation should still remain inactive).
  ASSERT_TRUE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());

  // Dictation enabled.

  ASSERT_TRUE(
      prefs->GetBoolean(prefs::kDictationAcceleratorDialogHasBeenAccepted));
  ASSERT_FALSE(controller->dictation_active());
  controller->dictation().SetEnabled(true);
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  // If Dictation is already on, then pressing the Dictation key should toggle
  // Dictation on/off.
  ASSERT_TRUE(controller->dictation().enabled());
  ASSERT_TRUE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());
  controller->EnableOrToggleDictationFromSource(
      DictationToggleSource::kKeyboard);
  ASSERT_TRUE(controller->dictation().enabled());
  ASSERT_FALSE(controller->dictation_active());
  ASSERT_FALSE(controller->IsDictationKeyboardDialogShowingForTesting());
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

  AccessibilityControllerSigninTest(const AccessibilityControllerSigninTest&) =
      delete;
  AccessibilityControllerSigninTest& operator=(
      const AccessibilityControllerSigninTest&) = delete;

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
};

}  // namespace

INSTANTIATE_TEST_SUITE_P(All,
                         AccessibilityControllerSigninTest,
                         ::testing::Values(TestUserLoginType::kNewUser,
                                           TestUserLoginType::kGuest,
                                           TestUserLoginType::kExistingUser));

TEST_P(AccessibilityControllerSigninTest, EnableOnLoginScreenAndLogin) {
  constexpr float kMagnifierScale = 4.3f;

  AccessibilityControllerImpl* accessibility =
      Shell::Get()->accessibility_controller();
  DockedMagnifierController* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();

  SessionControllerImpl* session = Shell::Get()->session_controller();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session->GetSessionState());
  EXPECT_FALSE(accessibility->large_cursor().enabled());
  EXPECT_FALSE(accessibility->live_caption().enabled());
  EXPECT_FALSE(accessibility->spoken_feedback().enabled());
  EXPECT_FALSE(accessibility->high_contrast().enabled());
  EXPECT_FALSE(accessibility->autoclick().enabled());
  EXPECT_FALSE(accessibility->mono_audio().enabled());
  EXPECT_FALSE(docked_magnifier->GetEnabled());
  using prefs::kAccessibilityAutoclickEnabled;
  using prefs::kAccessibilityHighContrastEnabled;
  using prefs::kAccessibilityLargeCursorEnabled;
  using prefs::kAccessibilityMonoAudioEnabled;
  using prefs::kAccessibilitySpokenFeedbackEnabled;
  using prefs::kDockedMagnifierEnabled;
  PrefService* signin_prefs = session->GetSigninScreenPrefService();
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
  EXPECT_FALSE(signin_prefs->GetBoolean(kDockedMagnifierEnabled));

  // Verify that toggling prefs at the signin screen changes the signin setting.
  accessibility->large_cursor().SetEnabled(true);
  accessibility->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  accessibility->high_contrast().SetEnabled(true);
  accessibility->autoclick().SetEnabled(true);
  accessibility->mono_audio().SetEnabled(true);
  docked_magnifier->SetEnabled(true);
  docked_magnifier->SetScale(kMagnifierScale);
  // TODO(afakhry): Test the Fullscreen magnifier prefs once the
  // ash::MagnificationController handles all the prefs work itself inside ash
  // without needing magnification manager in Chrome.
  EXPECT_TRUE(accessibility->large_cursor().enabled());
  EXPECT_TRUE(accessibility->spoken_feedback().enabled());
  EXPECT_TRUE(accessibility->high_contrast().enabled());
  EXPECT_TRUE(accessibility->autoclick().enabled());
  EXPECT_TRUE(accessibility->mono_audio().enabled());
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
    EXPECT_TRUE(accessibility->large_cursor().enabled());
    EXPECT_TRUE(accessibility->spoken_feedback().enabled());
    EXPECT_TRUE(accessibility->high_contrast().enabled());
    EXPECT_TRUE(accessibility->autoclick().enabled());
    EXPECT_TRUE(accessibility->mono_audio().enabled());
    EXPECT_TRUE(docked_magnifier->GetEnabled());
    EXPECT_FLOAT_EQ(kMagnifierScale, docked_magnifier->GetScale());
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityLargeCursorEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilitySpokenFeedbackEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityHighContrastEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityAutoclickEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilityMonoAudioEnabled));
    EXPECT_TRUE(user_prefs->GetBoolean(kDockedMagnifierEnabled));
  } else {
    EXPECT_FALSE(accessibility->large_cursor().enabled());
    EXPECT_FALSE(accessibility->spoken_feedback().enabled());
    EXPECT_FALSE(accessibility->high_contrast().enabled());
    EXPECT_FALSE(accessibility->autoclick().enabled());
    EXPECT_FALSE(accessibility->mono_audio().enabled());
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

TEST_P(AccessibilityControllerSigninTest, SwitchAccessPrefsSyncToSignIn) {
  AccessibilityControllerImpl* accessibility =
      Shell::Get()->accessibility_controller();

  SessionControllerImpl* session = Shell::Get()->session_controller();
  EXPECT_EQ(session_manager::SessionState::LOGIN_PRIMARY,
            session->GetSessionState());
  EXPECT_FALSE(accessibility->switch_access().enabled());
  using prefs::kAccessibilitySwitchAccessAutoScanEnabled;
  using prefs::kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs;
  using prefs::kAccessibilitySwitchAccessAutoScanSpeedMs;
  using prefs::kAccessibilitySwitchAccessEnabled;
  using prefs::kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond;

  PrefService* signin_prefs = session->GetSigninScreenPrefService();
  EXPECT_FALSE(signin_prefs->GetBoolean(kAccessibilitySwitchAccessEnabled));
  EXPECT_FALSE(
      signin_prefs->GetBoolean(kAccessibilitySwitchAccessAutoScanEnabled));

  SimulateLogin();

  PrefService* user_prefs = session->GetLastActiveUserPrefService();
  EXPECT_NE(signin_prefs, user_prefs);
  EXPECT_FALSE(user_prefs->GetBoolean(kAccessibilitySwitchAccessEnabled));
  EXPECT_FALSE(
      user_prefs->GetBoolean(kAccessibilitySwitchAccessAutoScanEnabled));

  // Enabling switch access in a user profile should enable it for the
  // signin profile.
  accessibility->switch_access().SetEnabled(true);
  EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilitySwitchAccessEnabled));
  EXPECT_TRUE(signin_prefs->GetBoolean(kAccessibilitySwitchAccessEnabled));

  // Changing a switch access option in a user profile should sync that
  // option to the signin profile.
  user_prefs->Set(kAccessibilitySwitchAccessAutoScanEnabled, base::Value(true));
  EXPECT_TRUE(
      signin_prefs->GetBoolean(kAccessibilitySwitchAccessAutoScanEnabled));

  user_prefs->Set(kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs,
                  base::Value(123));
  EXPECT_EQ(123, signin_prefs->GetInteger(
                     kAccessibilitySwitchAccessAutoScanKeyboardSpeedMs));

  user_prefs->Set(kAccessibilitySwitchAccessAutoScanSpeedMs, base::Value(234));
  EXPECT_EQ(
      234, signin_prefs->GetInteger(kAccessibilitySwitchAccessAutoScanSpeedMs));

  user_prefs->Set(kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond,
                  base::Value(345));
  EXPECT_EQ(345, signin_prefs->GetInteger(
                     kAccessibilitySwitchAccessPointScanSpeedDipsPerSecond));

  // The reverse is not true; turning off switch access in the signin profile
  // has no effect on the user profile.
  signin_prefs->Set(kAccessibilitySwitchAccessEnabled, base::Value(false));
  EXPECT_TRUE(user_prefs->GetBoolean(kAccessibilitySwitchAccessEnabled));
}

TEST_P(AccessibilityControllerSigninTest,
       UpdatesNonLoginWindowVisibilityOnLogin) {
  aura::Window* container =
      Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                          kShellWindowId_NonLockScreenContainersContainer);

  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  EXPECT_TRUE(
      container->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren));

  UnblockUserSession();
  EXPECT_FALSE(
      container->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren));

  BlockUserSession(BLOCKED_BY_LOGIN_SCREEN);
  EXPECT_TRUE(
      container->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren));

  UnblockUserSession();
  EXPECT_FALSE(
      container->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren));

  BlockUserSession(BLOCKED_BY_USER_ADDING_SCREEN);
  EXPECT_TRUE(
      container->GetProperty(ui::kAXConsiderInvisibleAndIgnoreChildren));
}

}  // namespace ash
