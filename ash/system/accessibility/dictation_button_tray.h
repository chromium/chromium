// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "ui/events/event_constants.h"

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
                                       public SessionObserver {
 public:
  explicit DictationButtonTray(Shelf* shelf);

  DictationButtonTray(const DictationButtonTray&) = delete;
  DictationButtonTray& operator=(const DictationButtonTray&) = delete;

  ~DictationButtonTray() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // ShellObserver:
  void OnDictationStarted() override;
  void OnDictationEnded() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // TrayBackgroundView:
  void Initialize() override;
  void ClickedOutsideBubble() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void OnThemeChanged() override;
  void Layout() override;

  // views::View:
  const char* GetClassName() const override;

  // Updates this button's state and progress indicator when speech recognition
  // file download state changes.
  void UpdateOnSpeechRecognitionDownloadChanged(int download_progress);

  int download_progress() const { return download_progress_; }

 private:
  friend class DictationButtonTrayTest;
  friend class DictationButtonTraySodaTest;

  // Sets the icon when Dictation is activated / deactivated.
  // Also updates visibility when Dictation is enabled / disabled.
  void UpdateIcon(bool dictation_active);

  // Updates opacity and transform for `icon_` to prevent overlap with the
  // `progress_indicator_` when downloading is in progress.
  void UpdateIconOpacityAndTransform();

  // Updates bounds for `progress_indicator_`.
  void UpdateProgressIndicatorBounds();

  // Updates the visibility of the button.
  void UpdateVisibility();

  // Actively looks up dictation status and calls UpdateIcon.
  void CheckDictationStatusAndUpdateIcon();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  // SODA download progress. A value of 0 < X < 100 indicates that download is
  // in-progress.
  int download_progress_;

  // A progress indicator to indicate SODA download progress and a subscription
  // to be notified of progress changed events.
  std::unique_ptr<ProgressIndicator> progress_indicator_;
  base::CallbackListSubscription progress_changed_subscription_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
