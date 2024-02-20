// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event_constants.h"

namespace ui {
class Event;
}  // namespace ui

namespace views {
class ImageView;
}  // namespace views

namespace ash {

class ProgressIndicator;

// Status area tray for showing a toggle for Dictation. Dictation allows
// users to have their speech transcribed into a text area. This tray will
// only be visible after Dictation is enabled in settings. This tray does not
// provide any bubble view windows.
class ASH_EXPORT DictationButtonTray : public TrayBackgroundView,
                                       public ShellObserver,
                                       public AccessibilityObserver,
                                       public SessionObserver,
                                       public ui::InputMethodObserver {
  METADATA_HEADER(DictationButtonTray, TrayBackgroundView)

 public:
  DictationButtonTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);
  DictationButtonTray(const DictationButtonTray&) = delete;
  DictationButtonTray& operator=(const DictationButtonTray&) = delete;
  ~DictationButtonTray() override;

  // ShellObserver:
  void OnDictationStarted() override;
  void OnDictationEnded() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TrayBackgroundView:
  void Initialize() override;
  void ClickedOutsideBubble(const ui::LocatedEvent& event) override;
  void UpdateTrayItemColor(bool is_active) override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void OnThemeChanged() override;
  void Layout(PassKey) override;
  void HideBubble(const TrayBubbleView* bubble_view) override;

  // ui::InputMethodObserver:
  void OnFocus() override {}
  void OnBlur() override {}
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override {}

  // Updates this button's state and progress indicator when speech recognition
  // file download state changes.
  void UpdateOnSpeechRecognitionDownloadChanged(int download_progress);

  int download_progress() const { return download_progress_; }

 private:
  friend class DictationButtonTrayTest;
  friend class DictationButtonTraySodaTest;

  // Callback called when this is pressed.
  void OnDictationButtonPressed(const ui::Event& event);

  // Sets the icon when Dictation is activated / deactivated.
  // Also updates visibility when Dictation is enabled / disabled.
  void UpdateIcon(bool dictation_active);

  // Updates bounds for `progress_indicator_`.
  void UpdateProgressIndicatorBounds();

  // Updates the visibility of the button.
  void UpdateVisibility();

  // Actively looks up dictation status and calls UpdateIcon.
  void CheckDictationStatusAndUpdateIcon();

  // Called when text input state changes to determine whether Dictation
  // should still be enabled and update the icon.
  void TextInputChanged(const ui::TextInputClient* client);

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  raw_ptr<views::ImageView> icon_ = nullptr;

  // SODA download progress. A value of 0 < X < 100 indicates that download is
  // in-progress.
  int download_progress_;

  // A progress indicator to indicate SODA download progress.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  // Whether the cursor and focus is currently in a text input field.
  bool in_text_input_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
