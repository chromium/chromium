// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/accessibility_detailed_view.h"

#include <memory>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/rounded_container.h"
#include "ash/style/switch.h"
#include "ash/system/tray/fake_detailed_view_delegate.h"
#include "ash/system/tray/hover_highlight_view.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "components/live_caption/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer_impl_chromeos.h"
#include "media/base/media_switches.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"

namespace ash {
namespace {

const std::u16string kInitialFeatureViewSubtitleText = u"This is a test";
const std::u16string kSodaDownloaded = u"Speech files downloaded";
const std::u16string kSodaInProgress25 =
    u"Downloading speech recognition files… 25%";
const std::u16string kSodaInProgress50 =
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

void EnableFaceGaze(bool enabled) {
  Shell::Get()->accessibility_controller()->face_gaze().SetEnabled(enabled);
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

void EnableColorCorrection(bool enabled) {
  Shell::Get()->accessibility_controller()->color_correction().SetEnabled(
      enabled);
}

void EnableReducedAnimations(bool enabled) {
  Shell::Get()->accessibility_controller()->reduced_animations().SetEnabled(
      enabled);
}

speech::LanguageCode en_us() {
  return speech::LanguageCode::kEnUs;
}

speech::LanguageCode fr_fr() {
  return speech::LanguageCode::kFrFr;
}

// Returns true if `view` is marked checked for accessibility.
bool IsCheckedForAccessibility(views::View* view) {
  ui::AXNodeData node_data;
  view->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  return node_data.GetCheckedState() == ax::mojom::CheckedState::kTrue;
}

// Returns true if `item` has a `Switch` on the right and the button is toggled.
bool IsSwitchToggled(HoverHighlightView* item) {
  views::View* right_view = item->right_view();
  if (!views::IsViewClass<Switch>(right_view)) {
    return false;
  }
  return static_cast<Switch*>(right_view)->GetIsOn();
}

}  // namespace

class AccessibilityDetailedViewTest : public AshTestBase,
                                      public AccessibilityObserver {
 public:
  AccessibilityDetailedViewTest() {
    scoped_feature_list_.InitWithFeatures(
        {ash::features::kOnDeviceSpeechRecognition,
         ::features::kAccessibilityFaceGaze,
         ::features::kAccessibilityReducedAnimationsInKiosk},
        {});
  }
  AccessibilityDetailedViewTest(const AccessibilityDetailedViewTest&) = delete;
  AccessibilityDetailedViewTest& operator=(
      const AccessibilityDetailedViewTest&) = delete;
  ~AccessibilityDetailedViewTest() override = default;

 protected:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = Shell::Get()->accessibility_controller();
    controller_->AddObserver(this);
  }

  void TearDown() override {
    CloseDetailMenu();
    controller_->RemoveObserver(this);
    controller_ = nullptr;
    AshTestBase::TearDown();
  }

  void CreateDetailedMenu() {
    // Create a widget for the detailed view so that tests can exercise focus.
    widget_ = CreateFramelessTestWidget();
    widget_->SetFullscreen(true);
    // Use a fake delegate to fake out CloseBubble() calls, since these tests
    // do not use a real system tray bubble.
    delegate_ = std::make_unique<FakeDetailedViewDelegate>();
    detailed_menu_ = widget_->SetContentsView(
        std::make_unique<AccessibilityDetailedView>(delegate_.get()));
  }

  void CloseDetailMenu() {
    widget_.reset();
    detailed_menu_ = nullptr;
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

  void ClickReducedAnimationsOnDetailMenu() {
    ClickView(detailed_menu_->reduced_animations_view_);
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

  void ClickFaceGazeOnDetailMenu() {
    ClickView(detailed_menu_->facegaze_view_);
  }

  void ClickColorCorrectionOnDetailMenu() {
    ClickView(detailed_menu_->color_correction_view_);
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

  bool IsFaceGazeShownOnDetailMenu() const {
    return detailed_menu_->facegaze_view_;
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

  bool IsReducedAnimationsShownOnDetailMenu() const {
    return detailed_menu_->reduced_animations_view_;
  }

  bool IsSwitchAccessShownOnDetailMenu() const {
    return detailed_menu_->switch_access_view_;
  }

  bool IsColorCorrectionShownOnDetailMenu() const {
    return detailed_menu_->color_correction_view_;
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
    bool checked_for_accessibility = IsCheckedForAccessibility(view);
    DCHECK_EQ(enabled_state, checked_for_accessibility);
    return enabled_state && checked_for_accessibility;
  }

  bool IsSpokenFeedbackEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->spoken_feedback().enabled(),
                                 detailed_menu_->spoken_feedback_view_);
  }

  bool IsSelectToSpeakEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->select_to_speak().enabled(),
                                 detailed_menu_->select_to_speak_view_);
  }

  bool IsDictationEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->dictation().enabled(),
                                 detailed_menu_->dictation_view_);
  }

  bool IsFaceGazeEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->face_gaze().enabled(),
                                 detailed_menu_->facegaze_view_);
  }

  bool IsHighContrastEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->high_contrast().enabled(),
                                 detailed_menu_->high_contrast_view_);
  }

  bool IsScreenMagnifierEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(
        Shell::Get()->accessibility_delegate()->IsMagnifierEnabled(),
        detailed_menu_->screen_magnifier_view_);
  }

  bool IsDockedMagnifierEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->docked_magnifier().enabled(),
                                 detailed_menu_->docked_magnifier_view_);
  }

  bool IsLargeCursorEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->large_cursor().enabled(),
                                 detailed_menu_->large_cursor_view_);
  }

  bool IsLiveCaptionEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->live_caption().enabled(),
                                 detailed_menu_->live_caption_view_);
  }

  bool IsAutoclickEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->autoclick().enabled(),
                                 detailed_menu_->autoclick_view_);
  }

  bool IsVirtualKeyboardEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->virtual_keyboard().enabled(),
                                 detailed_menu_->virtual_keyboard_view_);
  }

  bool IsMonoAudioEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->mono_audio().enabled(),
                                 detailed_menu_->mono_audio_view_);
  }

  bool IsCaretHighlightEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->caret_highlight().enabled(),
                                 detailed_menu_->caret_highlight_view_);
  }

  bool IsHighlightMouseCursorEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->cursor_highlight().enabled(),
                                 detailed_menu_->highlight_mouse_cursor_view_);
  }

  bool IsHighlightKeyboardFocusEnabledOnDetailMenu() const {
    // The highlight_keyboard_focus_view_ is not created when Spoken Feedback
    // is enabled.
    if (IsSpokenFeedbackEnabledOnDetailMenu()) {
      DCHECK(!detailed_menu_->highlight_keyboard_focus_view_);
      return false;
    }
    return IsEnabledOnDetailMenu(
        controller_->focus_highlight().enabled(),
        detailed_menu_->highlight_keyboard_focus_view_);
  }

  bool IsStickyKeysEnabledOnDetailMenu() const {
    // The sticky_keys_view_ is not created when Spoken Feedback is enabled.
    if (IsSpokenFeedbackEnabledOnDetailMenu()) {
      DCHECK(!detailed_menu_->sticky_keys_view_);
      return false;
    }
    return IsEnabledOnDetailMenu(controller_->sticky_keys().enabled(),
                                 detailed_menu_->sticky_keys_view_);
  }

  bool IsReducedAnimationsEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->reduced_animations().enabled(),
                                 detailed_menu_->reduced_animations_view_);
  }

  bool IsSwitchAccessEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->switch_access().enabled(),
                                 detailed_menu_->switch_access_view_);
  }

  bool IsColorCorrectionEnabledOnDetailMenu() const {
    return IsEnabledOnDetailMenu(controller_->color_correction().enabled(),
                                 detailed_menu_->color_correction_view_);
  }

  const char* GetDetailedViewClassName() {
    return detailed_menu_->GetClassName();
  }

  void SetUpKioskSession() {
    auto* session_controller = Shell::Get()->session_controller();
    SessionInfo info;
    info.state = session_controller->GetSessionState();
    info.is_running_in_app_mode = true;
    session_controller->SetSessionInfo(info);
  }

  AccessibilityController* controller() { return controller_; }
  AccessibilityDetailedView* detailed_menu() { return detailed_menu_; }
  views::View* scroll_content() { return detailed_menu_->scroll_content(); }

  // Accessors for list item views.
  views::View* spoken_feedback_view() const {
    return detailed_menu_->spoken_feedback_view_;
  }
  views::View* select_to_speak_view() const {
    return detailed_menu_->select_to_speak_view_;
  }
  views::View* dictation_view() const {
    return detailed_menu_->dictation_view_;
  }
  views::View* facegaze_view() const { return detailed_menu_->facegaze_view_; }
  views::View* high_contrast_view() const {
    return detailed_menu_->high_contrast_view_;
  }
  views::View* screen_magnifier_view() const {
    return detailed_menu_->screen_magnifier_view_;
  }
  views::View* docked_magnifier_view() const {
    return detailed_menu_->docked_magnifier_view_;
  }
  views::View* large_cursor_view() const {
    return detailed_menu_->large_cursor_view_;
  }
  views::View* live_caption_view() const {
    return detailed_menu_->live_caption_view_;
  }
  views::View* autoclick_view() const {
    return detailed_menu_->autoclick_view_;
  }
  views::View* virtual_keyboard_view() const {
    return detailed_menu_->virtual_keyboard_view_;
  }
  views::View* mono_audio_view() const {
    return detailed_menu_->mono_audio_view_;
  }
  views::View* caret_highlight_view() const {
    return detailed_menu_->caret_highlight_view_;
  }
  views::View* highlight_mouse_cursor_view() const {
    return detailed_menu_->highlight_mouse_cursor_view_;
  }
  views::View* highlight_keyboard_focus_view() const {
    return detailed_menu_->highlight_keyboard_focus_view_;
  }
  views::View* sticky_keys_view() const {
    return detailed_menu_->sticky_keys_view_;
  }
  views::View* reduced_animations_view() const {
    return detailed_menu_->reduced_animations_view_;
  }
  views::View* switch_access_view() const {
    return detailed_menu_->switch_access_view_;
  }
  views::View* color_correction_view() const {
    return detailed_menu_->color_correction_view_;
  }

  // Accessors for the top views listing enabled items.
  HoverHighlightView* spoken_feedback_top_view() const {
    return detailed_menu_->spoken_feedback_top_view_;
  }
  HoverHighlightView* select_to_speak_top_view() const {
    return detailed_menu_->select_to_speak_top_view_;
  }
  HoverHighlightView* dictation_top_view() const {
    return detailed_menu_->dictation_top_view_;
  }
  HoverHighlightView* facegaze_top_view() const {
    return detailed_menu_->facegaze_top_view_;
  }
  HoverHighlightView* high_contrast_top_view() const {
    return detailed_menu_->high_contrast_top_view_;
  }
  HoverHighlightView* screen_magnifier_top_view() const {
    return detailed_menu_->screen_magnifier_top_view_;
  }
  HoverHighlightView* docked_magnifier_top_view() const {
    return detailed_menu_->docked_magnifier_top_view_;
  }
  HoverHighlightView* large_cursor_top_view() const {
    return detailed_menu_->large_cursor_top_view_;
  }
  HoverHighlightView* live_caption_top_view() const {
    return detailed_menu_->live_caption_top_view_;
  }
  HoverHighlightView* autoclick_top_view() const {
    return detailed_menu_->autoclick_top_view_;
  }
  HoverHighlightView* virtual_keyboard_top_view() const {
    return detailed_menu_->virtual_keyboard_top_view_;
  }
  HoverHighlightView* mono_audio_top_view() const {
    return detailed_menu_->mono_audio_top_view_;
  }
  HoverHighlightView* caret_highlight_top_view() const {
    return detailed_menu_->caret_highlight_top_view_;
  }
  HoverHighlightView* highlight_mouse_cursor_top_view() const {
    return detailed_menu_->highlight_mouse_cursor_top_view_;
  }
  HoverHighlightView* highlight_keyboard_focus_top_view() const {
    return detailed_menu_->highlight_keyboard_focus_top_view_;
  }
  HoverHighlightView* sticky_keys_top_view() const {
    return detailed_menu_->sticky_keys_top_view_;
  }
  HoverHighlightView* reduced_animations_top_view() const {
    return detailed_menu_->reduced_animations_top_view_;
  }
  HoverHighlightView* switch_access_top_view() const {
    return detailed_menu_->switch_access_top_view_;
  }
  HoverHighlightView* color_correction_top_view() const {
    return detailed_menu_->color_correction_top_view_;
  }

 private:
  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override {
    // UnifiedAccessibilityDetailedViewController calls
    // AccessibilityDetailedView::OnAccessibilityStatusChanged. Spoof that
    // by calling it directly here.
    if (detailed_menu_) {
      detailed_menu_->OnAccessibilityStatusChanged();
    }
  }

  raw_ptr<AccessibilityController> controller_ = nullptr;
  std::unique_ptr<views::Widget> widget_;
  std::unique_ptr<DetailedViewDelegate> delegate_;
  raw_ptr<AccessibilityDetailedView, DanglingUntriaged> detailed_menu_ =
      nullptr;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AccessibilityDetailedViewTest, ListItemsAreInRoundedContainer) {
  CreateDetailedMenu();
  auto has_rounded_container_parent = [](views::View* view) -> bool {
    return views::IsViewClass<RoundedContainer>(view->parent());
  };
  EXPECT_TRUE(has_rounded_container_parent(spoken_feedback_view()));
  EXPECT_TRUE(has_rounded_container_parent(select_to_speak_view()));
  EXPECT_TRUE(has_rounded_container_parent(dictation_view()));
  EXPECT_TRUE(has_rounded_container_parent(high_contrast_view()));
  EXPECT_TRUE(has_rounded_container_parent(screen_magnifier_view()));
  EXPECT_TRUE(has_rounded_container_parent(docked_magnifier_view()));
  EXPECT_TRUE(has_rounded_container_parent(large_cursor_view()));
  EXPECT_TRUE(has_rounded_container_parent(live_caption_view()));
  EXPECT_TRUE(has_rounded_container_parent(autoclick_view()));
  EXPECT_TRUE(has_rounded_container_parent(virtual_keyboard_view()));
  EXPECT_TRUE(has_rounded_container_parent(mono_audio_view()));
  EXPECT_TRUE(has_rounded_container_parent(caret_highlight_view()));
  EXPECT_TRUE(has_rounded_container_parent(highlight_mouse_cursor_view()));
  EXPECT_TRUE(has_rounded_container_parent(highlight_keyboard_focus_view()));
  EXPECT_TRUE(has_rounded_container_parent(sticky_keys_view()));
  EXPECT_TRUE(has_rounded_container_parent(switch_access_view()));
  EXPECT_TRUE(has_rounded_container_parent(color_correction_view()));
  EXPECT_TRUE(has_rounded_container_parent(facegaze_view()));
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewTest, ContainerCount) {
  CreateDetailedMenu();
  // All features are disabled, so there should only be one container in the
  // scroll list, for the main item list.
  EXPECT_EQ(1u, scroll_content()->children().size());
  CloseDetailMenu();

  EnableSpokenFeedback(true);
  CreateDetailedMenu();
  // With one feature enabled, there should be two containers in the scroll
  // list, one for the top items and another one for the main item list.
  EXPECT_EQ(2u, scroll_content()->children().size());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewTest, TopsViewsAreEmptyWithNoFeaturesEnabled) {
  CreateDetailedMenu();

  // By default none of the accessibility features are enabled, so none of the
  // top-views are created.
  EXPECT_FALSE(spoken_feedback_top_view());
  EXPECT_FALSE(select_to_speak_top_view());
  EXPECT_FALSE(dictation_top_view());
  EXPECT_FALSE(high_contrast_top_view());
  EXPECT_FALSE(screen_magnifier_top_view());
  EXPECT_FALSE(docked_magnifier_top_view());
  EXPECT_FALSE(large_cursor_top_view());
  EXPECT_FALSE(live_caption_top_view());
  EXPECT_FALSE(autoclick_top_view());
  EXPECT_FALSE(virtual_keyboard_top_view());
  EXPECT_FALSE(mono_audio_top_view());
  EXPECT_FALSE(caret_highlight_top_view());
  EXPECT_FALSE(highlight_mouse_cursor_top_view());
  EXPECT_FALSE(highlight_keyboard_focus_top_view());
  EXPECT_FALSE(sticky_keys_top_view());
  EXPECT_FALSE(switch_access_top_view());
  EXPECT_FALSE(color_correction_top_view());
  EXPECT_FALSE(facegaze_top_view());
}

// Verifies that pressing the tab key moves from row to row. In particular,
// this verifies that the toggle button does not take focus.
TEST_F(AccessibilityDetailedViewTest, TabMovesFocusBetweenRows) {
  CreateDetailedMenu();
  spoken_feedback_view()->RequestFocus();
  EXPECT_TRUE(spoken_feedback_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(select_to_speak_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(dictation_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(facegaze_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(color_correction_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(high_contrast_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(screen_magnifier_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(docked_magnifier_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(autoclick_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(virtual_keyboard_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(switch_access_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(live_caption_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(large_cursor_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(mono_audio_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(caret_highlight_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(highlight_mouse_cursor_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(highlight_keyboard_focus_view()->HasFocus());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(sticky_keys_view()->HasFocus());
}

TEST_F(AccessibilityDetailedViewTest, SpokenFeedbackTopView) {
  EnableSpokenFeedback(true);
  CreateDetailedMenu();
  ASSERT_TRUE(spoken_feedback_top_view());
  EXPECT_TRUE(IsSwitchToggled(spoken_feedback_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(spoken_feedback_top_view()));

  ClickView(spoken_feedback_top_view());
  EXPECT_FALSE(IsSwitchToggled(spoken_feedback_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(spoken_feedback_top_view()));
  EXPECT_FALSE(controller()->spoken_feedback().enabled());
}

TEST_F(AccessibilityDetailedViewTest, SelectToSpeakTopView) {
  EnableSelectToSpeak(true);
  CreateDetailedMenu();
  ASSERT_TRUE(select_to_speak_top_view());
  EXPECT_TRUE(IsSwitchToggled(select_to_speak_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(select_to_speak_top_view()));

  ClickView(select_to_speak_top_view());
  EXPECT_FALSE(IsSwitchToggled(select_to_speak_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(select_to_speak_top_view()));
  EXPECT_FALSE(controller()->select_to_speak().enabled());
}

TEST_F(AccessibilityDetailedViewTest, DictationTopView) {
  EnableDictation(true);
  CreateDetailedMenu();
  ASSERT_TRUE(dictation_top_view());
  EXPECT_TRUE(IsSwitchToggled(dictation_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(dictation_top_view()));

  ClickView(dictation_top_view());
  EXPECT_FALSE(IsSwitchToggled(dictation_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(dictation_top_view()));
  EXPECT_FALSE(controller()->dictation().enabled());
}

TEST_F(AccessibilityDetailedViewTest, FaceGazeTopView) {
  EnableFaceGaze(true);
  CreateDetailedMenu();
  ASSERT_TRUE(facegaze_top_view());
  EXPECT_TRUE(IsSwitchToggled(facegaze_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(facegaze_top_view()));

  ClickView(facegaze_top_view());
  EXPECT_FALSE(IsSwitchToggled(facegaze_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(facegaze_top_view()));
  EXPECT_FALSE(controller()->face_gaze().enabled());
}

TEST_F(AccessibilityDetailedViewTest, HighContrastTopView) {
  EnableHighContrast(true);
  CreateDetailedMenu();
  ASSERT_TRUE(high_contrast_top_view());
  EXPECT_TRUE(IsSwitchToggled(high_contrast_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(high_contrast_top_view()));

  ClickView(high_contrast_top_view());
  EXPECT_FALSE(IsSwitchToggled(high_contrast_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(high_contrast_top_view()));
  EXPECT_FALSE(controller()->high_contrast().enabled());
}

TEST_F(AccessibilityDetailedViewTest, ScreenMagnifierTopView) {
  Shell::Get()->accessibility_delegate()->SetMagnifierEnabled(true);
  CreateDetailedMenu();
  ASSERT_TRUE(screen_magnifier_top_view());
  EXPECT_TRUE(IsSwitchToggled(screen_magnifier_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(screen_magnifier_top_view()));

  ClickView(screen_magnifier_top_view());
  // The test accessibility delegate doesn't notify observers of changes, so do
  // it manually.
  controller()->NotifyAccessibilityStatusChanged();

  EXPECT_FALSE(IsSwitchToggled(screen_magnifier_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(screen_magnifier_top_view()));
  EXPECT_FALSE(Shell::Get()->accessibility_delegate()->IsMagnifierEnabled());
}

TEST_F(AccessibilityDetailedViewTest, DockedMagnifierTopView) {
  SetDockedMagnifierEnabled(true);
  CreateDetailedMenu();
  ASSERT_TRUE(docked_magnifier_top_view());
  EXPECT_TRUE(IsSwitchToggled(docked_magnifier_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(docked_magnifier_top_view()));

  ClickView(docked_magnifier_top_view());
  EXPECT_FALSE(IsSwitchToggled(docked_magnifier_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(docked_magnifier_top_view()));
  EXPECT_FALSE(Shell::Get()->docked_magnifier_controller()->GetEnabled());
}

TEST_F(AccessibilityDetailedViewTest, LargeCursorTopView) {
  EnableLargeCursor(true);
  CreateDetailedMenu();
  ASSERT_TRUE(large_cursor_top_view());
  EXPECT_TRUE(IsSwitchToggled(large_cursor_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(large_cursor_top_view()));

  ClickView(large_cursor_top_view());
  EXPECT_FALSE(IsSwitchToggled(large_cursor_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(large_cursor_top_view()));
  EXPECT_FALSE(controller()->large_cursor().enabled());
}

TEST_F(AccessibilityDetailedViewTest, LiveCaptionTopView) {
  EnableLiveCaption(true);
  CreateDetailedMenu();
  ASSERT_TRUE(live_caption_top_view());
  EXPECT_TRUE(IsSwitchToggled(live_caption_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(live_caption_top_view()));

  ClickView(live_caption_top_view());
  EXPECT_FALSE(IsSwitchToggled(live_caption_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(live_caption_top_view()));
  EXPECT_FALSE(controller()->live_caption().enabled());
}

TEST_F(AccessibilityDetailedViewTest, AutoClickTopView) {
  EnableAutoclick(true);
  CreateDetailedMenu();
  ASSERT_TRUE(autoclick_top_view());
  EXPECT_TRUE(IsSwitchToggled(autoclick_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(autoclick_top_view()));

  ClickView(autoclick_top_view());
  EXPECT_FALSE(IsSwitchToggled(autoclick_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(autoclick_top_view()));
  EXPECT_FALSE(controller()->autoclick().enabled());
}

TEST_F(AccessibilityDetailedViewTest, VirtualKeyboardTopView) {
  EnableVirtualKeyboard(true);
  CreateDetailedMenu();
  ASSERT_TRUE(virtual_keyboard_top_view());
  EXPECT_TRUE(IsSwitchToggled(virtual_keyboard_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(virtual_keyboard_top_view()));

  ClickView(virtual_keyboard_top_view());
  EXPECT_FALSE(IsSwitchToggled(virtual_keyboard_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(virtual_keyboard_top_view()));
  EXPECT_FALSE(controller()->virtual_keyboard().enabled());
}

TEST_F(AccessibilityDetailedViewTest, MonoAudioTopView) {
  EnableMonoAudio(true);
  CreateDetailedMenu();
  ASSERT_TRUE(mono_audio_top_view());
  EXPECT_TRUE(IsSwitchToggled(mono_audio_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(mono_audio_top_view()));

  ClickView(mono_audio_top_view());
  EXPECT_FALSE(IsSwitchToggled(mono_audio_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(mono_audio_top_view()));
  EXPECT_FALSE(controller()->mono_audio().enabled());
}

TEST_F(AccessibilityDetailedViewTest, CaretHighlightTopView) {
  SetCaretHighlightEnabled(true);
  CreateDetailedMenu();
  ASSERT_TRUE(caret_highlight_top_view());
  EXPECT_TRUE(IsSwitchToggled(caret_highlight_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(caret_highlight_top_view()));

  ClickView(caret_highlight_top_view());
  EXPECT_FALSE(IsSwitchToggled(caret_highlight_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(caret_highlight_top_view()));
  EXPECT_FALSE(controller()->caret_highlight().enabled());
}

TEST_F(AccessibilityDetailedViewTest, HighlightMouseCursorTopView) {
  SetCursorHighlightEnabled(true);
  CreateDetailedMenu();
  ASSERT_TRUE(highlight_mouse_cursor_top_view());
  EXPECT_TRUE(IsSwitchToggled(highlight_mouse_cursor_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(highlight_mouse_cursor_top_view()));

  ClickView(highlight_mouse_cursor_top_view());
  EXPECT_FALSE(IsSwitchToggled(highlight_mouse_cursor_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(highlight_mouse_cursor_top_view()));
  EXPECT_FALSE(controller()->cursor_highlight().enabled());
}

TEST_F(AccessibilityDetailedViewTest, HighlightKeyboardFocusTopView) {
  SetFocusHighlightEnabled(true);
  CreateDetailedMenu();
  ASSERT_TRUE(highlight_keyboard_focus_top_view());
  EXPECT_TRUE(IsSwitchToggled(highlight_keyboard_focus_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(highlight_keyboard_focus_top_view()));

  ClickView(highlight_keyboard_focus_top_view());
  EXPECT_FALSE(IsSwitchToggled(highlight_keyboard_focus_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(highlight_keyboard_focus_top_view()));
  EXPECT_FALSE(controller()->focus_highlight().enabled());
}

TEST_F(AccessibilityDetailedViewTest, StickyKeysTopView) {
  EnableStickyKeys(true);
  CreateDetailedMenu();
  ASSERT_TRUE(sticky_keys_top_view());
  EXPECT_TRUE(IsSwitchToggled(sticky_keys_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(sticky_keys_top_view()));

  ClickView(sticky_keys_top_view());
  EXPECT_FALSE(IsSwitchToggled(sticky_keys_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(sticky_keys_top_view()));
  EXPECT_FALSE(controller()->sticky_keys().enabled());
}

TEST_F(AccessibilityDetailedViewTest, SwitchAccessTopView) {
  // Don't show the confirmation dialog when disabling switch access, so the
  // feature will be disabled immediately.
  controller()->DisableSwitchAccessDisableConfirmationDialogTesting();

  EnableSwitchAccess(true);
  CreateDetailedMenu();
  ASSERT_TRUE(switch_access_top_view());
  EXPECT_TRUE(IsSwitchToggled(switch_access_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(switch_access_top_view()));

  ClickView(switch_access_top_view());
  EXPECT_FALSE(IsSwitchToggled(switch_access_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(switch_access_top_view()));
  EXPECT_FALSE(controller()->switch_access().enabled());
}

TEST_F(AccessibilityDetailedViewTest, ColorCorrectionTopView) {
  EnableColorCorrection(true);
  CreateDetailedMenu();
  ASSERT_TRUE(color_correction_top_view());
  EXPECT_TRUE(IsSwitchToggled(color_correction_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(color_correction_top_view()));

  ClickView(color_correction_top_view());
  EXPECT_FALSE(IsSwitchToggled(color_correction_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(color_correction_top_view()));
  EXPECT_FALSE(controller()->color_correction().enabled());
}

TEST_F(AccessibilityDetailedViewTest, CheckMenuVisibilityOnDetailMenu) {
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
  EXPECT_TRUE(IsColorCorrectionShownOnDetailMenu());
  EXPECT_TRUE(IsFaceGazeShownOnDetailMenu());
  // Reduced animations not available outside of kiosk.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
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
  EXPECT_TRUE(IsColorCorrectionShownOnDetailMenu());
  EXPECT_TRUE(IsFaceGazeShownOnDetailMenu());
  // Reduced animations not available outside of kiosk.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
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
  EXPECT_TRUE(IsColorCorrectionShownOnDetailMenu());
  EXPECT_TRUE(IsFaceGazeShownOnDetailMenu());
  // Reduced animations not available outside of kiosk.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
  UnblockUserSession();
}

TEST_F(AccessibilityDetailedViewTest, ClickDetailMenu) {
  AccessibilityController* accessibility_controller =
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

  // Confirms that the check item toggles color correction.
  EXPECT_FALSE(accessibility_controller->color_correction().enabled());

  CreateDetailedMenu();
  ClickColorCorrectionOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->color_correction().enabled());

  CreateDetailedMenu();
  ClickColorCorrectionOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->color_correction().enabled());

  // Confirms that the check item toggles color correction.
  EXPECT_FALSE(accessibility_controller->face_gaze().enabled());

  CreateDetailedMenu();
  ClickFaceGazeOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->face_gaze().enabled());

  CreateDetailedMenu();
  ClickFaceGazeOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->face_gaze().enabled());
}

TEST_F(AccessibilityDetailedViewTest, KioskModeShowsReducedAnimations) {
  SetUpKioskSession();

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
  EXPECT_TRUE(IsColorCorrectionShownOnDetailMenu());
  EXPECT_TRUE(IsFaceGazeShownOnDetailMenu());
  EXPECT_TRUE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewTest, KioskModeReducedAnimationsView) {
  SetUpKioskSession();
  // Enabling reduced animations.
  EnableReducedAnimations(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  ASSERT_TRUE(IsReducedAnimationsShownOnDetailMenu());
  EXPECT_TRUE(IsReducedAnimationsEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling reduced animations.
  EnableReducedAnimations(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  ASSERT_TRUE(IsReducedAnimationsShownOnDetailMenu());
  EXPECT_FALSE(IsReducedAnimationsEnabledOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewTest, KioskModeReducedAnimationsTopView) {
  SetUpKioskSession();
  EnableReducedAnimations(true);
  CreateDetailedMenu();
  ASSERT_TRUE(reduced_animations_top_view());
  EXPECT_TRUE(IsSwitchToggled(reduced_animations_top_view()));
  EXPECT_TRUE(IsCheckedForAccessibility(reduced_animations_top_view()));

  ClickView(reduced_animations_top_view());
  EXPECT_FALSE(IsSwitchToggled(reduced_animations_top_view()));
  EXPECT_FALSE(IsCheckedForAccessibility(reduced_animations_top_view()));
  EXPECT_FALSE(controller()->reduced_animations().enabled());
}

TEST_F(AccessibilityDetailedViewTest, KioskModeClickReducedAnimations) {
  SetUpKioskSession();

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();
  // Confirms that the check item toggles reduced animations.
  EXPECT_FALSE(accessibility_controller->reduced_animations().enabled());

  CreateDetailedMenu();
  ClickReducedAnimationsOnDetailMenu();
  EXPECT_TRUE(accessibility_controller->reduced_animations().enabled());

  CreateDetailedMenu();
  ClickReducedAnimationsOnDetailMenu();
  EXPECT_FALSE(accessibility_controller->reduced_animations().enabled());
}

class AccessibilityDetailedViewSodaTest
    : public AccessibilityDetailedViewTest,
      public testing::WithParamInterface<SodaFeature> {
 public:
  AccessibilityDetailedViewSodaTest() { set_start_session(false); }
  AccessibilityDetailedViewSodaTest(const AccessibilityDetailedViewSodaTest&) =
      delete;
  AccessibilityDetailedViewSodaTest& operator=(
      const AccessibilityDetailedViewSodaTest&) = delete;
  ~AccessibilityDetailedViewSodaTest() override = default;

  void SetUp() override {
    AccessibilityDetailedViewTest::SetUp();
    // Since this test suite is part of ash unit tests, the
    // SodaInstallerImplChromeOS is never created (it's normally created when
    // `ChromeBrowserMainPartsAsh` initializes). Create it here so that
    // calling speech::SodaInstaller::GetInstance() returns a valid instance.
    std::vector<base::test::FeatureRef> enabled_features(
        {ash::features::kOnDeviceSpeechRecognition});
    if (GetParam() == SodaFeature::kLiveCaption) {
      enabled_features.push_back(media::kLiveCaptionMultiLanguage);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, {});
    soda_installer_impl_ =
        std::make_unique<speech::SodaInstallerImplChromeOS>();

    CreateDetailedMenu();
    EnableFeature(true);
    SetFeatureViewSubtitleText(kInitialFeatureViewSubtitleText);
    SetFeatureLocale("en-US");
  }

  void TearDown() override {
    soda_installer_impl_.reset();
    AccessibilityDetailedViewTest::TearDown();
  }

  void EnableFeature(bool enabled) {
    switch (GetParam()) {
      case SodaFeature::kDictation:
        EnableDictation(enabled);
        break;
      case SodaFeature::kLiveCaption:
        EnableLiveCaption(enabled);
        break;
    }
  }

  void SetFeatureLocale(const std::string& locale) {
    switch (GetParam()) {
      case SodaFeature::kDictation:
        Shell::Get()->session_controller()->GetActivePrefService()->SetString(
            prefs::kAccessibilityDictationLocale, locale);
        break;
      case SodaFeature::kLiveCaption:
        Shell::Get()->session_controller()->GetActivePrefService()->SetString(
            ::prefs::kLiveCaptionLanguageCode, locale);
        break;
    }
  }

  speech::SodaInstaller* soda_installer() {
    return speech::SodaInstaller::GetInstance();
  }

  void SetFeatureViewSubtitleText(std::u16string text) {
    switch (GetParam()) {
      case SodaFeature::kDictation:
        detailed_menu()->dictation_view_->SetSubText(text);
        break;
      case SodaFeature::kLiveCaption:
        detailed_menu()->live_caption_view_->SetSubText(text);
        break;
    }
  }

  std::u16string GetFeatureViewSubtitleText() {
    switch (GetParam()) {
      case SodaFeature::kDictation:
        return detailed_menu()->dictation_view_->sub_text_label()->GetText();
      case SodaFeature::kLiveCaption:
        return detailed_menu()->live_caption_view_->sub_text_label()->GetText();
    }
  }

 private:
  std::unique_ptr<speech::SodaInstallerImplChromeOS> soda_installer_impl_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         AccessibilityDetailedViewSodaTest,
                         ::testing::Values(SodaFeature::kDictation,
                                           SodaFeature::kLiveCaption));

// Ensures that the feature subtitle changes when SODA AND the language pack
// matching the feature locale are installed.
TEST_P(AccessibilityDetailedViewSodaTest, OnSodaInstalledNotification) {
  SetFeatureLocale("fr-FR");

  // Pretend that the SODA binary was installed. We still need to wait for the
  // correct language pack before doing anything.
  soda_installer()->NotifySodaInstalledForTesting();
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(en_us());
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting(fr_fr());
  EXPECT_EQ(kSodaDownloaded, GetFeatureViewSubtitleText());
}

// Ensures we only notify the user of progress for the language pack matching
// the feature locale.
TEST_P(AccessibilityDetailedViewSodaTest, OnSodaProgressNotification) {
  SetFeatureLocale("en-US");

  soda_installer()->NotifySodaProgressForTesting(75, fr_fr());
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50);
  EXPECT_EQ(kSodaInProgress50, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(25, en_us());
  EXPECT_EQ(kSodaInProgress25, GetFeatureViewSubtitleText());
}

// Ensures we notify the user of an error when the SODA binary fails to
// download.
TEST_P(AccessibilityDetailedViewSodaTest, SodaBinaryErrorNotification) {
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kSodaFailed, GetFeatureViewSubtitleText());
}

// Ensures we only notify the user of an error if the failed language pack
// matches the feature locale.
TEST_P(AccessibilityDetailedViewSodaTest, SodaLanguageErrorNotification) {
  SetFeatureLocale("en-US");
  soda_installer()->NotifySodaErrorForTesting(fr_fr());
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting(en_us());
  EXPECT_EQ(kSodaFailed, GetFeatureViewSubtitleText());
}

// Ensures that we don't respond to SODA download updates when the feature is
// off.
TEST_P(AccessibilityDetailedViewSodaTest, SodaDownloadFeatureDisabled) {
  EnableFeature(false);
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaErrorForTesting();
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaInstalledForTesting();
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
  soda_installer()->NotifySodaProgressForTesting(50);
  EXPECT_EQ(kInitialFeatureViewSubtitleText, GetFeatureViewSubtitleText());
}

class AccessibilityDetailedViewLoginScreenTest
    : public AccessibilityDetailedViewTest {
 public:
  AccessibilityDetailedViewLoginScreenTest(
      const AccessibilityDetailedViewLoginScreenTest&) = delete;
  AccessibilityDetailedViewLoginScreenTest& operator=(
      const AccessibilityDetailedViewLoginScreenTest&) = delete;

 protected:
  AccessibilityDetailedViewLoginScreenTest() { set_start_session(false); }
  ~AccessibilityDetailedViewLoginScreenTest() override = default;
};

TEST_F(AccessibilityDetailedViewLoginScreenTest, NothingCheckedByDefault) {
  // At first, all of the check is unchecked.
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, SpokenFeedback) {
  // Enabling spoken feedback.
  EnableSpokenFeedback(true);
  CreateDetailedMenu();
  EXPECT_TRUE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling spoken feedback.
  EnableSpokenFeedback(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest,
       SpokenFeedbackConflictingFeatures) {
  EnableStickyKeys(true);
  SetFocusHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  CloseDetailMenu();

  // When ChromeVox is on, even though sticky keys and focus highlight were
  // enabled, they will not be shown.
  EnableSpokenFeedback(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_FALSE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  CloseDetailMenu();

  EnableSpokenFeedback(false);
  CreateDetailedMenu();
  EXPECT_TRUE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsHighlightKeyboardFocusEnabledOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, SelectToSpeak) {
  // Enabling select to speak.
  EnableSelectToSpeak(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_TRUE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling select to speak.
  EnableSelectToSpeak(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, Dictation) {
  // Enabling dictation.
  EnableDictation(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_TRUE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling dictation.
  EnableDictation(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, HighContrast) {
  // Enabling high contrast.
  EnableHighContrast(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling high contrast.
  EnableHighContrast(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, FullScreenMagnifier) {
  // Enabling full screen magnifier.
  SetScreenMagnifierEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling screen magnifier.
  SetScreenMagnifierEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, DockedMagnifier) {
  // Enabling docked magnifier.
  SetDockedMagnifierEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling docked magnifier.
  SetDockedMagnifierEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, LargeCursor) {
  // Enabling large cursor.
  EnableLargeCursor(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling large cursor.
  EnableLargeCursor(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, LiveCaption) {
  // Enabling Live Caption.
  EnableLiveCaption(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling Live Caption.
  EnableLiveCaption(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, VirtualKeyboard) {
  // Enable on-screen keyboard.
  EnableVirtualKeyboard(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disable on-screen keyboard.
  EnableVirtualKeyboard(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, MonoAudio) {
  // Enabling mono audio.
  EnableMonoAudio(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling mono audio.
  EnableMonoAudio(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, CaretHighlight) {
  // Enabling caret highlight.
  SetCaretHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling caret highlight.
  SetCaretHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, CursorHighlight) {
  // Enabling highlight mouse cursor.
  SetCursorHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight mouse cursor.
  SetCursorHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, FocusHighlight) {
  // Enabling highlight keyboard focus.
  SetFocusHighlightEnabled(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling highlight keyboard focus.
  SetFocusHighlightEnabled(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, StickyKeys) {
  // Enabling sticky keys.
  EnableStickyKeys(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling sticky keys.
  EnableStickyKeys(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

// Switch Access is currently not available on the login screen; see
// crbug/1108808
/*
TEST_F(AccessibilityDetailedViewLoginScreenTest, SwitchAccess) {
  // Enabling switch access.
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
}
*/

TEST_F(AccessibilityDetailedViewLoginScreenTest, AllFeatures) {
  // Enabling all of the a11y features.
  EnableSpokenFeedback(true);
  EnableSelectToSpeak(true);
  EnableDictation(true);
  EnableFaceGaze(true);
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
  EXPECT_TRUE(IsFaceGazeEnabledOnDetailMenu());
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
  // Sticky keys can't be on when spoken feedback is on.
  EXPECT_FALSE(IsStickyKeysEnabledOnDetailMenu());
  EXPECT_TRUE(IsSwitchAccessEnabledOnDetailMenu());
  CloseDetailMenu();

  // Disabling all of the a11y features.
  EnableSpokenFeedback(false);
  EnableSelectToSpeak(false);
  EnableDictation(false);
  EnableFaceGaze(false);
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
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, Autoclick) {
  // Enabling autoclick.
  EnableAutoclick(true);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling autoclick.
  EnableAutoclick(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

TEST_F(AccessibilityDetailedViewLoginScreenTest, FaceGaze) {
  // Enabling facegaze.
  EnableFaceGaze(true);
  CreateDetailedMenu();
  EXPECT_TRUE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();

  // Disabling facegaze.
  EnableFaceGaze(false);
  CreateDetailedMenu();
  EXPECT_FALSE(IsSpokenFeedbackEnabledOnDetailMenu());
  EXPECT_FALSE(IsSelectToSpeakEnabledOnDetailMenu());
  EXPECT_FALSE(IsDictationEnabledOnDetailMenu());
  EXPECT_FALSE(IsFaceGazeEnabledOnDetailMenu());
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
  // TODO(crbug.com/40707666): Uncomment once issue is addressed.
  // EXPECT_FALSE(IsSwitchAccessEnabledOnDetailMenu());
  // Color correction cannot be enabled from the login screen.
  EXPECT_FALSE(IsColorCorrectionShownOnDetailMenu());
  // Reduced animations not available from the login screen.
  EXPECT_FALSE(IsReducedAnimationsShownOnDetailMenu());
  CloseDetailMenu();
}

}  // namespace ash
