// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_
#define ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_

#include <stdint.h>

#include "ash/accessibility/accessibility_delegate.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/system/tray/tray_detailed_view.h"
#include "components/soda/soda_installer.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace chromeos {
class TrayAccessibilityTest;
}

namespace views {
class Button;
class Button;
class View;
}  // namespace views

namespace ash {
class HoverHighlightView;
class DetailedViewDelegate;
class TrayAccessibilityLoginScreenTest;
class TrayAccessibilitySodaTest;
class TrayAccessibilityTest;

enum class SodaFeature {
  kDictation,
  kLiveCaption,
};

// Create the detailed view of accessibility tray.
class ASH_EXPORT AccessibilityDetailedView
    : public TrayDetailedView,
      public speech::SodaInstaller::Observer {
 public:
  static const char kClassName[];

  explicit AccessibilityDetailedView(DetailedViewDelegate* delegate);

  AccessibilityDetailedView(const AccessibilityDetailedView&) = delete;
  AccessibilityDetailedView& operator=(const AccessibilityDetailedView&) =
      delete;

  ~AccessibilityDetailedView() override;

  void OnAccessibilityStatusChanged();

  // views::View
  const char* GetClassName() const override;

 private:
  friend class TrayAccessibilityLoginScreenTest;
  friend class TrayAccessibilitySodaTest;
  friend class TrayAccessibilityTest;
  friend class chromeos::TrayAccessibilityTest;

  // TrayDetailedView:
  void HandleViewClicked(views::View* view) override;
  void CreateExtraTitleRowButtons() override;

  // Launches the WebUI settings in a browser and closes the system menu.
  void ShowSettings();

  // Launches the a11y help article in a browser and closes the system menu.
  void ShowHelp();

  // Add the accessibility feature list.
  void AppendAccessibilityList();

  // SodaInstaller::Observer:
  void OnSodaInstalled(speech::LanguageCode language_code) override;
  void OnSodaError(speech::LanguageCode language_code) override;
  void OnSodaProgress(speech::LanguageCode language_code,
                      int combined_progress) override;

  // Shows a message next to the feature icon in the tray if it is available
  // and if the language code provided is relevant to the feature.
  void MaybeShowSodaMessage(SodaFeature feature,
                            speech::LanguageCode language_code,
                            std::u16string message);
  bool IsSodaFeatureInTray(SodaFeature feature);
  void SetSodaFeatureSubtext(SodaFeature feature, std::u16string message);

  HoverHighlightView* spoken_feedback_view_ = nullptr;
  HoverHighlightView* select_to_speak_view_ = nullptr;
  HoverHighlightView* dictation_view_ = nullptr;
  HoverHighlightView* high_contrast_view_ = nullptr;
  HoverHighlightView* screen_magnifier_view_ = nullptr;
  HoverHighlightView* docked_magnifier_view_ = nullptr;
  HoverHighlightView* large_cursor_view_ = nullptr;
  HoverHighlightView* autoclick_view_ = nullptr;
  HoverHighlightView* virtual_keyboard_view_ = nullptr;
  HoverHighlightView* switch_access_view_ = nullptr;
  HoverHighlightView* live_caption_view_ = nullptr;
  HoverHighlightView* mono_audio_view_ = nullptr;
  HoverHighlightView* caret_highlight_view_ = nullptr;
  HoverHighlightView* highlight_mouse_cursor_view_ = nullptr;
  HoverHighlightView* highlight_keyboard_focus_view_ = nullptr;
  HoverHighlightView* sticky_keys_view_ = nullptr;
  views::Button* help_view_ = nullptr;
  views::Button* settings_view_ = nullptr;

  // These exist for tests. The canonical state is stored in prefs.
  bool spoken_feedback_enabled_ = false;
  bool select_to_speak_enabled_ = false;
  bool dictation_enabled_ = false;
  bool high_contrast_enabled_ = false;
  bool screen_magnifier_enabled_ = false;
  bool docked_magnifier_enabled_ = false;
  bool large_cursor_enabled_ = false;
  bool autoclick_enabled_ = false;
  bool virtual_keyboard_enabled_ = false;
  bool switch_access_enabled_ = false;
  bool live_caption_enabled_ = false;
  bool mono_audio_enabled_ = false;
  bool caret_highlight_enabled_ = false;
  bool highlight_mouse_cursor_enabled_ = false;
  bool highlight_keyboard_focus_enabled_ = false;
  bool sticky_keys_enabled_ = false;

  LoginStatus login_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_TRAY_ACCESSIBILITY_H_
