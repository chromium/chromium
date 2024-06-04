// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_DETAILED_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_DETAILED_VIEW_H_

#include <stdint.h>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "base/memory/raw_ptr.h"
#include "components/soda/soda_installer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace views {
class Button;
class Button;
class View;
}  // namespace views

namespace ash {
class HoverHighlightView;
class DetailedViewDelegate;

enum class SodaFeature {
  kDictation,
  kLiveCaption,
};

// Create the detailed view of accessibility tray.
class ASH_EXPORT AccessibilityDetailedView
    : public TrayDetailedView,
      public speech::SodaInstaller::Observer {
  METADATA_HEADER(AccessibilityDetailedView, TrayDetailedView)

 public:
  explicit AccessibilityDetailedView(DetailedViewDelegate* delegate);

  AccessibilityDetailedView(const AccessibilityDetailedView&) = delete;
  AccessibilityDetailedView& operator=(const AccessibilityDetailedView&) =
      delete;

  ~AccessibilityDetailedView() override;

  void OnAccessibilityStatusChanged();

 private:
  friend class AccessibilityDetailedViewLoginScreenTest;
  friend class AccessibilityDetailedViewSodaTest;
  friend class AccessibilityDetailedViewTest;
  friend class TrayAccessibilityTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Launches the a11y help article in a browser and closes the system menu.
  void ShowHelp();

  // Add the accessibility feature list.
  void AppendAccessibilityList();

  // Adds the enabled accessibility features to the `container`.
  void AddEnabledFeatures(views::View* container);

  // Adds all accessibility features to the `container`.
  void AddAllFeatures(views::View* container);

  // Adds a HoverHighlightView to `container` for the appropriate accessibility
  // feature and returns it.
  HoverHighlightView* AddSpokenFeedbackView(views::View* container);
  HoverHighlightView* AddSelectToSpeakView(views::View* container);
  HoverHighlightView* AddDictationView(views::View* container);
  HoverHighlightView* AddFaceGazeView(views::View* container);
  HoverHighlightView* AddColorCorrectionView(views::View* container);
  HoverHighlightView* AddHighContrastView(views::View* container);
  HoverHighlightView* AddScreenMagnifierView(views::View* container);
  HoverHighlightView* AddDockedMagnifierView(views::View* container);
  HoverHighlightView* AddLargeCursorView(views::View* container);
  HoverHighlightView* AddAutoclickView(views::View* container);
  HoverHighlightView* AddVirtualKeyboardView(views::View* container);
  HoverHighlightView* AddSwitchAccessView(views::View* container);
  HoverHighlightView* AddLiveCaptionView(views::View* container);
  HoverHighlightView* AddMonoAudioView(views::View* container);
  HoverHighlightView* AddCaretHighlightView(views::View* container);
  HoverHighlightView* AddHighlightMouseCursorView(views::View* container);
  HoverHighlightView* AddHighlightKeyboardFocusView(views::View* container);
  HoverHighlightView* AddStickyKeysView(views::View* container);
  HoverHighlightView* AddReducedAnimationsView(views::View* container);

  // Adds a HoverHighlightView to the scroll list and returns it.
  HoverHighlightView* AddScrollListFeatureItem(views::View* container,
                                               const gfx::VectorIcon& icon,
                                               const std::u16string& text,
                                               bool checked,
                                               bool enterprise_managed);

  // Adds a HoverHighlightView with a toggle button on the right to the scroll
  // list and returns it.
  HoverHighlightView* AddScrollListToggleItem(views::View* container,
                                              const gfx::VectorIcon& icon,
                                              const std::u16string& text,
                                              bool checked,
                                              bool enterprise_managed);

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaInstallError(speech::LanguageCode language_code,
                          speech::SodaInstaller::ErrorCode error_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override;

  // Shows a message next to the feature icon in the tray if it is available
  // and if the language code provided is relevant to the feature.
  void MaybeShowSodaMessage(SodaFeature feature,
                            speech::LanguageCode language_code,
                            std::u16string message);
  bool IsSodaFeatureInTray(SodaFeature feature);
  void SetSodaFeatureSubtext(SodaFeature feature, std::u16string message);

  raw_ptr<HoverHighlightView> spoken_feedback_view_ = nullptr;
  raw_ptr<HoverHighlightView> select_to_speak_view_ = nullptr;
  raw_ptr<HoverHighlightView> dictation_view_ = nullptr;
  raw_ptr<HoverHighlightView> facegaze_view_ = nullptr;
  raw_ptr<HoverHighlightView> color_correction_view_ = nullptr;
  raw_ptr<HoverHighlightView> high_contrast_view_ = nullptr;
  raw_ptr<HoverHighlightView> screen_magnifier_view_ = nullptr;
  raw_ptr<HoverHighlightView> docked_magnifier_view_ = nullptr;
  raw_ptr<HoverHighlightView> large_cursor_view_ = nullptr;
  raw_ptr<HoverHighlightView> autoclick_view_ = nullptr;
  raw_ptr<HoverHighlightView> virtual_keyboard_view_ = nullptr;
  raw_ptr<HoverHighlightView> switch_access_view_ = nullptr;
  raw_ptr<HoverHighlightView> live_caption_view_ = nullptr;
  raw_ptr<HoverHighlightView> mono_audio_view_ = nullptr;
  raw_ptr<HoverHighlightView> caret_highlight_view_ = nullptr;
  raw_ptr<HoverHighlightView> highlight_mouse_cursor_view_ = nullptr;
  raw_ptr<HoverHighlightView> highlight_keyboard_focus_view_ = nullptr;
  raw_ptr<HoverHighlightView> sticky_keys_view_ = nullptr;
  raw_ptr<HoverHighlightView> reduced_animations_view_ = nullptr;

  // Views that appear in the top section listing enabled items. Created if the
  // feature is enabled, otherwise nullptr. Owned by views hierarchy.
  raw_ptr<HoverHighlightView> spoken_feedback_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> select_to_speak_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> dictation_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> facegaze_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> color_correction_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> high_contrast_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> screen_magnifier_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> docked_magnifier_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> large_cursor_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> autoclick_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> virtual_keyboard_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> switch_access_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> live_caption_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> mono_audio_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> caret_highlight_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> highlight_mouse_cursor_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> highlight_keyboard_focus_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> sticky_keys_top_view_ = nullptr;
  raw_ptr<HoverHighlightView> reduced_animations_top_view_ = nullptr;

  raw_ptr<views::Button> help_view_ = nullptr;
  raw_ptr<views::Button> settings_view_ = nullptr;

  LoginStatus login_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_ACCESSIBILITY_DETAILED_VIEW_H_
