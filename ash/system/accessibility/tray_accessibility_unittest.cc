// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/tray_accessibility.h"
#include <memory>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"

namespace ash {
namespace {

const std::u16string kInitialDictationViewSubtitleText = u"This is a test";
const std::u16string kSodaDownloaded = u"Speech files downloaded";
const std::u16string kSodaInProgress =
    u"Downloading speech recognition files… 50%";
const std::u16string kSodaFailed =
    u"Can't download speech files. Try again later.";

void SetScreenMagnifierEnabled(bool enabled) {
  Shell::Get()->accessibility_delegate()->SetMagnifierEnabled(enabled);
}

void SetDockedMagnifierEnabled(bool enabled) {
  Shell::Get()->accessibility_controller()->docked_magnifier().SetEnabled(
      enabled);
}

void EnableSpokenFeedback(bool enabled) {
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      enabled, A11Y_NOTIFICATION_NONE);
}

void EnableSelectToSpeak(bool enabled) {
  Shell::Get()->accessibility_controller()->select_to_speak().SetEnabled(
      enabled);
}

void EnableDictation(bool enabled) {
  Shell::Get()->accessibility_controller()->dictation().SetEnabled(enabled);
}

void EnableHighContrast(bool enabled) {
  Shell::Get()->accessibility_controller()->high_contrast().SetEnabled(enabled);
}

void EnableAutoclick(bool enabled) {
  Shell::Get()->accessibility_controller()->autoclick().SetEnabled(enabled);
}

void EnableVirtualKeyboard(bool enabled) {
  Shell::Get()->accessibility_controller()->virtual_keyboard().SetEnabled(
      enabled);
}

void EnableLargeCursor(bool enabled) {
  Shell::Get()->accessibility_controller()->large_cursor().SetEnabled(enabled);
}

void EnableLiveCaption(bool enabled) {
  Shell::Get()->accessibility_controller()->live_caption().SetEnabled(enabled);
}

void EnableMonoAudio(bool enabled) {
  Shell::Get()->accessibility_controller()->mono_audio().SetEnabled(enabled);
}

void SetCaretHighlightEnabled(bool enabled) {
  Shell::Get()->accessibility_controller()->caret_highlight().SetEnabled(
      enabled);
}

void SetCursorHighlightEnabled(bool enabled) {
  Shell::Get()->accessibility_controller()->cursor_highlight().SetEnabled(
      enabled);
}

void SetFocusHighlightEnabled(bool enabled) {
  Shell::Get()->accessibility_controller()->focus_highlight().SetEnabled(
      enabled);
}

void EnableStickyKeys(bool enabled) {
  Shell::Get()->accessibility_controller()->sticky_keys().SetEnabled(enabled);
}

void EnableSwitchAccess(bool enabled) {
  Shell::Get()->accessibility_controller()->switch_access().SetEnabled(enabled);
}

}  // namespace

class TrayAccessibilityTest : public AshTestBase, public AccessibilityObserver {
 public:
  TrayAccessibilityTest(const TrayAccessibilityTest&) = delete;
  TrayAccessibilityTest& operator=(const TrayAccessibilityTest&) = delete;

 protected:
  TrayAccessibilityTest() = default;
  ~TrayAccessibilityTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {media::kLiveCaption, media::kLiveCaptionSystemWideOnChromeOS,
         ash::features::kOnDeviceSpeechRecognition},
        {});
    AshTestBase::SetUp();
    Shell::Get()->accessibility_controller()->AddObserver(this);
  }

  void TearDown() override {
    Shell::Get()->accessibility_controller()->RemoveObserver(this);
    AshTestBase::TearDown();
  }

  void CreateDetailedMenu() {
    delegate_ = std::make_unique<DetailedViewDelegate>(nullptr);
    detailed_menu_ =
        std::make_unique<tray::AccessibilityDetailedView>(delegate_.get());
  }

  void CloseDetailMenu() {
    detailed_menu_.reset();
    delegate_.reset();
  }

  void ClickView(HoverHighlightView* view) {
    detailed_menu_->OnViewClicked(view);
  }

  // These helpers may change prefs in ash, so they must spin the message loop
  // to wait for chrome to observe the change.
  void ClickSpokenFeedbackOnDetailMenu() {
    ClickView(detailed_menu_->spoken_feedback_view_);
  }

  void ClickHighContrastOnDetailMenu() {
    ClickView(detailed_menu_->high_contrast_view_);
  }

  void ClickScreenMagnifierOnDetailMenu() {
    ClickView(detailed_menu_->screen_magnifier_view_);
  }

  void ClickDockedMagnifierOnDetailMenu() {
    ClickView(detailed_menu_->docked_magnifier_view_);
  }

  void ClickAutoclickOnDetailMenu() {
    ClickView(detailed_menu_->autoclick_view_);
  }

  void ClickVirtualKeyboardOnDetailMenu() {
    ClickView(detailed_menu_->virtual_keyboard_view_);
  }

  void ClickLargeMouseCursorOnDetailMenu() {
    ClickView(detailed_menu_->large_cursor_view_);
  }

  void ClickLiveCaptionOnDetailMenu() {
    ClickView(detailed_menu_->live_caption_view_);
  }

  void ClickMonoAudioOnDetailMenu() {
    ClickView(detailed_menu_->mono_audio_view_);
  }

  void ClickCaretHighlightOnDetailMenu() {
    ClickView(detailed_menu_->caret_highlight_view_);
  }

  void ClickHighlightMouseCursorOnDetailMenu() {
    ClickView(detailed_menu_->highlight_mouse_cursor_view_);
  }

  void ClickHighlightKeyboardFocusOnDetailMenu() {
    ClickView(detailed_menu_->highlight_keyboard_focus_view_);
  }

  void ClickStickyKeysOnDetailMenu() {
    ClickView(detailed_menu_->sticky_keys_view_);
  }

  void ClickSwitchAccessOnDetailMenu() {
    ClickView(detailed_menu_->switch_access_view_);
  }

  void ClickSelectToSpeakOnDetailMenu() {
    ClickView(detailed_menu_->select_to_speak_view_);
  }

  void ClickDictationOnDetailMenu() {
    ClickView(detailed_menu_->dictation_view_);
  }

  bool IsSpokenFeedbackMenuShownOnDetailMenu() const {
    return detailed_menu_->spoken_feedback_view_;
  }

  bool IsSelectToSpeakShownOnDetailMenu() const {
    return detailed_menu_->select_to_speak_view_;
  }

  bool IsDictationShownOnDetailMenu() const {
    return detailed_menu_->dictation_view_;
  }

  bool IsHighContrastMenuShownOnDetailMenu() const {
    return detailed_menu_->high_contrast_view_;
  }

  bool IsScreenMagnifierMenuShownOnDetailMenu() const {
    return detailed_menu_->screen_magnifier_view_;
  }

  bool IsDockedMagnifierShownOnDetailMenu() const {
    return detailed_menu_->docked_magnifier_view_;
  }

  bool IsLargeCursorMenuShownOnDetailMenu() const {
    return detailed_menu_->large_cursor_view_;
  }

  bool IsLiveCaptionShownOnDetailMenu() const {
    return detailed_menu_->live_caption_view_;
  }

  bool IsAutoclickMenuShownOnDetailMenu() const {
    return detailed_menu_->autoclick_view_;
  }

  bool IsVirtualKeyboardMenuShownOnDetailMenu() const {
    return detailed_menu_->virtual_keyboard_view_;
  }

  bool IsMonoAudioMenuShownOnDetailMenu() const {
    return detailed_menu_->mono_audio_view_;
  }

  bool IsCaretHighlightMenuShownOnDetailMenu() const {
    return detailed_menu_->caret_highlight_view_;
  }

  bool IsHighlightMouseCursorMenuShownOnDetailMenu() const {
    return detailed_menu_->highlight_mouse_cursor_view_;
  }

  bool IsHighlightKeyboardFocusMenuShownOnDetailMenu() const {
    return detailed_menu_->highlight_keyboard_focus_view_;
  }

  bool IsStickyKeysMenuShownOnDetailMenu() const {
    return detailed_menu_->sticky_keys_view_;
  }

  bool IsSwitchAccessShownOnDetailMenu() const {
    return detailed_menu_->switch_access_view_;
  }

  // In material design we show the help button but theme it as disabled if
  // it is not possible to load the help page.
  bool IsHelpAvailableOnDetailMenu() {
    return detailed_menu_->help_view_->GetState() ==
           views::Button::STATE_NORMAL;
  }

  // In material design we show the settings button but theme it as disabled if
  // it is not possible to load the settings page.
  bool IsSettingsAvailableOnDetailMenu() {
    return detailed_menu_->settings_view_->GetState() ==
           views::Button::STATE_NORMAL;
  }

  // An item is enabled on the detailed menu if it is marked checked for
  // accessibility and the detailed_menu_'s local state, |enabled_state|, is
  // enabled. Check that the checked state and detailed_menu_'s local state are
  // the same.
  bool IsEnabledOnDetailMenu(bool enabled_state, views::View* view) const {
    ui::AXNodeData node_data;
    view->GetAccessibleNodeData(&node_data);
    bool checked_for_accessibility =
        node_data.GetCheckedState() == ax::mojom::CheckedState::kTrue;
    DCHECK(enabled_state == checked_for_accessibility);
    return enabled_state && checked_for_accessibility;
  }

  bool IsSpokenFeedbackEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->spoken_feedback_enabled_,
                                 detailed_menu_->spoken_feedback_view_);
  }

  bool IsSelectToSpeakEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->select_to_speak_enabled_,
                                 detailed_menu_->select_to_speak_view_);
  }

  bool IsDictationEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->dictation_enabled_,
                                 detailed_menu_->dictation_view_);
  }

  bool IsHighContrastEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->high_contrast_enabled_,
                                 detailed_menu_->high_contrast_view_);
  }

  bool IsScreenMagnifierEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->screen_magnifier_enabled_,
                                 detailed_menu_->screen_magnifier_view_);
  }

  bool IsDockedMagnifierEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->docked_magnifier_enabled_,
                                 detailed_menu_->docked_magnifier_view_);
  }

  bool IsLargeCursorEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->large_cursor_enabled_,
                                 detailed_menu_->large_cursor_view_);
  }

  bool IsLiveCaptionEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->live_caption_enabled_,
                                 detailed_menu_->live_caption_view_);
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->autoclick_enabled_,
                                 detailed_menu_->autoclick_view_);
  }

  bool IsVirtualKeyboardEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->virtual_keyboard_enabled_,
                                 detailed_menu_->virtual_keyboard_view_);
  }

  bool IsMonoAudioEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->mono_audio_enabled_,
                                 detailed_menu_->mono_audio_view_);
  }

  bool IsCaretHighlightEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->caret_highlight_enabled_,
                                 detailed_menu_->caret_highlight_view_);
  }

  bool IsHighlightMouseCursorEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(
        detailed_menu_->highlight_mouse_cursor_enabled_,
        detailed_menu_->highlight_mouse_cursor_view_);
  }

  bool IsHighlightKeyboardFocusEnabledOnDetailMenu() const {
    // The highlight_keyboard_focus_view_ is not created when Spoken Feedback
    // is enabled.
    if (IsSpokenFeedbackEnabledOnDetailMenu()) {
      DCHECK(!detailed_menu_->highlight_keyboard_focus_view_);
      return detailed_menu_->highlight_keyboard_focus_enabled_;
    }
    return IsEnabledOnDetailMenu(
        detailed_menu_->highlight_keyboard_focus_enabled_,
        detailed_menu_->highlight_keyboard_focus_view_);
  }

  bool IsStickyKeysEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->sticky_keys_enabled_,
                                 detailed_menu_->sticky_keys_view_);
  }

  bool IsSwitchAccessEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(detailed_menu_->switch_access_enabled_,
                                 detailed_menu_->switch_access_view_);
  }

  const char* GetDetailedViewClassName() {
    return detailed_menu_->GetClassName();
  }

  tray::AccessibilityDetailedView* detailed_menu() {
    return detailed_menu_.get();
  }

 private:
  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override {
    // UnifiedAccessibilityDetailedViewController calls
    // AccessibilityDetailedView::OnAccessibilityStatusChanged. Spoof that
    // by calling it directly here.
    if (detailed_menu_)
      detailed_menu_->OnAccessibilityStatusChanged();
  }

  std::unique_ptr<DetailedViewDelegate> delegate_;
  std::unique_ptr<tray::AccessibilityDetailedView> detailed_menu_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(TrayAccessibilityTest, CheckMenuVisibilityOnDetailMenu) {
  // Except help & settings, others should be kept the same
  // in LOGIN | NOT LOGIN | LOCKED. https://crbug.com/632107.
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsDictationShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsDockedMagnifierShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHelpAvailableOnDetailMenu());
  EXPECT_TRUE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsLiveCaptionShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessShownOnDetailMenu());
  CloseDetailMenu();

  // Simulate screen lock.
  BlockUserSession(BLOCKED_BY_LOCK_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsDictationShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsDockedMagnifierShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsLiveCaptionShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessShownOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();

  // Simulate adding multiprofile user.
  BlockUserSession(BLOCKED_BY_USER_ADDING_SCREEN);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakShownOnDetailMenu());
  EXPECT_TRUE(IsDictationShownOnDetailMenu());
  EXPECT_TRUE(IsHighContrastMenuShownOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierMenuShownOnDetailMenu());
  EXPECT_TRUE(IsDockedMagnifierShownOnDetailMenu());
  EXPECT_TRUE(IsAutoclickMenuShownOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardMenuShownOnDetailMenu());
  EXPECT_FALSE(IsHelpAvailableOnDetailMenu());
  EXPECT_FALSE(IsSettingsAvailableOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsLiveCaptionShownOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioMenuShownOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorMenuShownOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusMenuShownOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysMenuShownOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessShownOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();
}

TEST_F(TrayAccessibilityTest, ClickDetailMenu) {
  AccessibilityControllerImpl* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Confirms that the check item toggles the spoken feedback.
  EXPECT_FALSE(accessibility_controller->spoken_feedback().enabled());

  CreateDetailedMenu();
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->spoken_feedback().enabled());

  CreateDetailedMenu();
  ClickSpokenFeedbackOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->spoken_feedback().enabled());

  // Confirms that the check item toggles the high contrast.
  EXPECT_FALSE(accessibility_controller->high_contrast().enabled());

  CreateDetailedMenu();
  ClickHighContrastOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->high_contrast().enabled());

  CreateDetailedMenu();
  ClickHighContrastOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->high_contrast().enabled());

  // Confirms that the check item toggles the magnifier.
  EXPECT_FALSE(Shell::Get()->accessibility_delegate()->IsMagnifierEnabled());

  CreateDetailedMenu();
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_TRUE(Shell::Get()->accessibility_delegate()->IsMagnifierEnabled());

  CreateDetailedMenu();
  ClickScreenMagnifierOnDetailMenu();
  EXPECT_FALSE(Shell::Get()->accessibility_delegate()->IsMagnifierEnabled());

  // Confirms that the check item toggles the docked magnifier.
  EXPECT_FALSE(Shell::Get()->docked_magnifier_controller()->GetEnabled());

  CreateDetailedMenu();
  ClickDockedMagnifierOnDetailMenu();
  EXPECT_TRUE(Shell::Get()->docked_magnifier_controller()->GetEnabled());

  CreateDetailedMenu();
  ClickDockedMagnifierOnDetailMenu();
  EXPECT_FALSE(Shell::Get()->docked_magnifier_controller()->GetEnabled());

  // Confirms that the check item toggles autoclick.
  EXPECT_FALSE(accessibility_controller->autoclick().enabled());

  CreateDetailedMenu();
  ClickAutoclickOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->autoclick().enabled());

  CreateDetailedMenu();
  ClickAutoclickOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->autoclick().enabled());

  // Confirms that the check item toggles on-screen keyboard.
  EXPECT_FALSE(accessibility_controller->virtual_keyboard().enabled());

  CreateDetailedMenu();
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->virtual_keyboard().enabled());

  CreateDetailedMenu();
  ClickVirtualKeyboardOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->virtual_keyboard().enabled());

  // Confirms that the check item toggles large mouse cursor.
  EXPECT_FALSE(accessibility_controller->large_cursor().enabled());

  CreateDetailedMenu();
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->large_cursor().enabled());

  CreateDetailedMenu();
  ClickLargeMouseCursorOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->large_cursor().enabled());

  // Confirms that the check item toggles Live Caption.
  EXPECT_FALSE(accessibility_controller->live_caption().enabled());

  CreateDetailedMenu();
  ClickLiveCaptionOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->live_caption().enabled());

  CreateDetailedMenu();
  ClickLiveCaptionOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->live_caption().enabled());

  // Confirms that the check item toggles mono audio.
  EXPECT_FALSE(accessibility_controller->mono_audio().enabled());

  CreateDetailedMenu();
  ClickMonoAudioOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->mono_audio().enabled());

  CreateDetailedMenu();
  ClickMonoAudioOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->mono_audio().enabled());

  // Confirms that the check item toggles caret highlight.
  EXPECT_FALSE(accessibility_controller->caret_highlight().enabled());

  CreateDetailedMenu();
  ClickCaretHighlightOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->caret_highlight().enabled());

  CreateDetailedMenu();
  ClickCaretHighlightOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->caret_highlight().enabled());

  // Confirms that the check item toggles highlight mouse cursor.
  EXPECT_FALSE(accessibility_controller->cursor_highlight().enabled());

  CreateDetailedMenu();
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->cursor_highlight().enabled());

  CreateDetailedMenu();
  ClickHighlightMouseCursorOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->cursor_highlight().enabled());

  // Confirms that the check item toggles highlight keyboard focus.
  EXPECT_FALSE(accessibility_controller->focus_highlight().enabled());

  CreateDetailedMenu();
  ClickHighlightKeyboardFocusOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->focus_highlight().enabled());

  CreateDetailedMenu();
  ClickHighlightKeyboardFocusOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->focus_highlight().enabled());

  // Confirms that the check item toggles sticky keys.
  EXPECT_FALSE(accessibility_controller->sticky_keys().enabled());

  CreateDetailedMenu();
  ClickStickyKeysOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->sticky_keys().enabled());

  CreateDetailedMenu();
  ClickStickyKeysOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->sticky_keys().enabled());

  // Confirms that the check item toggles switch access.
  EXPECT_FALSE(accessibility_controller->switch_access().enabled());

  CreateDetailedMenu();
  ClickSwitchAccessOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->switch_access().enabled());

  CreateDetailedMenu();
  ClickSwitchAccessOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->switch_access().enabled());

  // Confirms that the check item toggles select-to-speak.
  EXPECT_FALSE(accessibility_controller->select_to_speak().enabled());

  CreateDetailedMenu();
  ClickSelectToSpeakOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->select_to_speak().enabled());

  CreateDetailedMenu();
  ClickSelectToSpeakOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->select_to_speak().enabled());

  // Confirms that the check item toggles dictation.
  EXPECT_FALSE(accessibility_controller->dictation().enabled());

  CreateDetailedMenu();
  ClickDictationOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->dictation().enabled());

  CreateDetailedMenu();
  ClickDictationOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->dictation().enabled());
}

// Trivial test to increase code coverage.
TEST_F(TrayAccessibilityTest, GetClassName) {
  CreateDetailedMenu();
  EXPECT_EQ(tray::AccessibilityDetailedView::kClassName,
            GetDetailedViewClassName());
}

class TrayAccessibilitySodaTest : public TrayAccessibilityTest {
 protected:
  TrayAccessibilitySodaTest() { set_start_session(false); }
  ~TrayAccessibilitySodaTest() override = default;
  TrayAccessibilitySodaTest(const TrayAccessibilitySodaTest&) = delete;
  TrayAccessibilitySodaTest& operator=(const TrayAccessibilitySodaTest&) =
      delete;

  void SetUp() override {
    TrayAccessibilityTest::SetUp();
    // Since this test suite is part of ash unit tests, the
    // SodaInstallerImplChromeOS is never created (it's normally created when
    // `ChromeBrowserMainPartsAsh` initializes). Create it here so that
    // calling speech::SodaInstaller::GetInstance() returns a valid instance.
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOnDeviceSpeechRecognition}, {});
    soda_installer_impl_ =
        std::make_unique<speech::SodaInstallerImplChromeOS>();
    soda_installer()->UninstallSodaForTesting();

    CreateDetailedMenu();
    EnableDictation(true);
    SetDictationViewSubtitleText(kInitialDictationViewSubtitleText);
    SetDictationLocale("en-US");
  }

  void TearDown() override {
    soda_installer()->UninstallSodaForTesting();
    soda_installer_impl_.reset();
    TrayAccessibilityTest::TearDown();
  }

  void SetDictationLocale(const std::string& locale) {
    Shell::Get()->session_controller()->GetActivePrefService()->SetString(
        prefs::kAccessibilityDictationLocale, locale);
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  speech::LanguageCode en_us() { return speech::LanguageCode::kEnUs; }
  speech::LanguageCode fr_fr() { return speech::LanguageCode::kFrFr; }

  void SetDictationViewSubtitleText(std::u16string text) {
    detailed_menu()->SetDictationViewSubtitleTextForTesting(text);
  }

  std::u16string GetDictationViewSubtitleText() {
    return detailed_menu()->GetDictationViewSubtitleTextForTesting();
  }

 private:
  std::unique_ptr<speech::SodaInstallerImplChromeOS> soda_installer_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Ensures that the Dictation subtitle changes when SODA AND the language pack
// matching the Dictation locale are installed.
TEST_F(TrayAccessibilitySodaTest, OnSodaInstalledNotification) {
  SetDictationLocale("fr-FR");

  // Pretend that the SODA binary was installed. We still need to wait for the
  // correct language pack before doing anything.
  soda_installer()->NotifySodaInstalledForTesting();
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(fr_fr());
  EXPECT_EQ(kSodaDownloaded, GetDictationViewSubtitleText());
}

// Ensures we only notify the user of progress for the language pack matching
// the Dictation locale.
TEST_F(TrayAccessibilitySodaTest, OnSodaProgressNotification) {
  soda_installer()->NotifySodaProgressForTesting(50, fr_fr());
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50);
  EXPECT_EQ(kSodaInProgress, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50, en_us());
  EXPECT_EQ(kSodaInProgress, GetDictationViewSubtitleText());
}

// Ensures we only notify the user of an error when the SODA binary fails to
// download.
TEST_F(TrayAccessibilitySodaTest, SodaBinaryErrorNotification) {
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kSodaFailed, GetDictationViewSubtitleText());
}

TEST_F(TrayAccessibilitySodaTest, SodaLanguageErrorNotification) {
  // Do nothing if the failed language pack is different than the Dictation
  // locale.
  soda_installer()->NotifySodaErrorForTesting(fr_fr());
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting(en_us());
  EXPECT_EQ(kSodaFailed, GetDictationViewSubtitleText());
}

// Ensures that we don't respond to SODA download updates when dictation is off.
TEST_F(TrayAccessibilitySodaTest, SodaDownloadDictationDisabled) {
  EnableDictation(false);
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kInitialDictationViewSubtitleText, GetDictationViewSubtitleText());
}

class TrayAccessibilityLoginScreenTest : public TrayAccessibilityTest {
 public:
  TrayAccessibilityLoginScreenTest(const TrayAccessibilityLoginScreenTest&) =
      delete;
  TrayAccessibilityLoginScreenTest& operator=(
      const TrayAccessibilityLoginScreenTest&) = delete;

 protected:
  TrayAccessibilityLoginScreenTest() { set_start_session(false); }
  ~TrayAccessibilityLoginScreenTest() override = default;
};

TEST_F(TrayAccessibilityLoginScreenTest, CheckMarksOnDetailMenu) {
  // At first, all of the check is unchecked.
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling spoken feedback.
  EnableSpokenFeedback(true);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling spoken feedback.
  EnableSpokenFeedback(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling select to speak.
  EnableSelectToSpeak(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling select to speak.
  EnableSelectToSpeak(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling dictation.
  EnableDictation(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_TRUE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling dictation.
  EnableDictation(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling high contrast.
  EnableHighContrast(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling high contrast.
  EnableHighContrast(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling full screen magnifier.
  SetScreenMagnifierEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling screen magnifier.
  SetScreenMagnifierEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling docked magnifier.
  SetDockedMagnifierEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling docked magnifier.
  SetDockedMagnifierEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling large cursor.
  EnableLargeCursor(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling large cursor.
  EnableLargeCursor(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling Live Caption.
  EnableLiveCaption(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling Live Caption.
  EnableLiveCaption(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enable on-screen keyboard.
  EnableVirtualKeyboard(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disable on-screen keyboard.
  EnableVirtualKeyboard(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling mono audio.
  EnableMonoAudio(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling mono audio.
  EnableMonoAudio(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling caret highlight.
  SetCaretHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling caret highlight.
  SetCaretHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight mouse cursor.
  SetCursorHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight keyboard focus.
  SetFocusHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling sticky keys.
  EnableStickyKeys(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling sticky keys.
  EnableStickyKeys(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Switch Access is currently not available on the login screen; see
  // crbug/1108808
  /* // Enabling switch access.
  EnableSwitchAccess(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling switch access.
  EnableSwitchAccess(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();
  */

  // Enabling all of the a11y features.
  EnableSpokenFeedback(true);
  EnableSelectToSpeak(true);
  EnableDictation(true);
  EnableHighContrast(true);
  SetScreenMagnifierEnabled(true);
  SetDockedMagnifierEnabled(true);
  EnableLargeCursor(true);
  EnableLiveCaption(true);
  EnableVirtualKeyboard(true);
  EnableAutoclick(true);
  EnableMonoAudio(true);
  SetCaretHighlightEnabled(true);
  SetCursorHighlightEnabled(true);
  SetFocusHighlightEnabled(true);
  EnableStickyKeys(true);
  EnableSwitchAccess(true);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_TRUE(IsDictationEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_TRUE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_TRUE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_TRUE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_TRUE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_TRUE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_TRUE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightMouseCursorEnabledOnDetailMenu());
  // Focus highlighting can't be on when spoken feedback is on
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  EnableSpokenFeedback(false);
  EnableSelectToSpeak(false);
  EnableDictation(false);
  EnableHighContrast(false);
  SetScreenMagnifierEnabled(false);
  SetDockedMagnifierEnabled(false);
  EnableLargeCursor(false);
  EnableLiveCaption(false);
  EnableVirtualKeyboard(false);
  EnableAutoclick(false);
  EnableMonoAudio(false);
  SetCaretHighlightEnabled(false);
  SetCursorHighlightEnabled(false);
  SetFocusHighlightEnabled(false);
  EnableStickyKeys(false);
  EnableSwitchAccess(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently cannot be enabled from the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Enabling autoclick.
  EnableAutoclick(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_TRUE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently not available on the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling autoclick.
  EnableAutoclick(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighContrastEnabledOnDetailMenu());
  EXPECT_FALSE(IsScreenMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsDockedMagnifierEnabledOnDetailMenu());
  EXPECT_FALSE(IsLargeCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsLiveCaptionEnabledOnDetailMenu());
  EXPECT_FALSE(IsAutoclickEnabledOnDetailMenu());
  EXPECT_FALSE(IsVirtualKeyboardEnabledOnDetailMenu());
  EXPECT_FALSE(IsMonoAudioEnabledOnDetailMenu());
  EXPECT_FALSE(IsCaretHighlightEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightMouseCursorEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  // Switch Access is currently not available on the login screen.
  // TODO(crbug.com/1108808): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();
}

}  // namespace ash
