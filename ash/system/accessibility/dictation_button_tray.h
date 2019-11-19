// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "ui/events/event_constants.h"

namespace views {
class ImageView;
}

namespace ash {

// Status area tray for showing a toggle for Dictation. Dictation allows
// users to have their speech transcribed into a text area. This tray will
// only be visible after Dictation is enabled in settings. This tray does not
// provide any bubble view windows.
class ASH_EXPORT DictationButtonTray : public TrayBackgroundView,
                                       public ShellObserver,
                                       public AccessibilityObserver {
 public:
  explicit DictationButtonTray(Shelf* shelf);
  ~DictationButtonTray() override;

  // ActionableView:
  bool PerformAction(const ui::Event& event) override;

  // ShellObserver:
  void OnDictationStarted() override;
  void OnDictationEnded() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // TrayBackgroundView:
  void ClickedOutsideBubble() override;
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  friend class DictationButtonTrayTest;

  // Sets the icon when Dictation is activated / deactiviated.
  // Also updates visibility when Dictation is enabled / disabled.
  void UpdateIcon(bool dictation_active);

  // Updates the visibility of the button.
  // Currently the button is visible iff experimental accessibility
  // features are enabled.
  void UpdateVisibility();

  // Actively looks up dictation status and calls UpdateIcon.
  void CheckDictationStatusAndUpdateIcon();

  gfx::ImageSkia on_image_;
  gfx::ImageSkia off_image_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  DISALLOW_COPY_AND_ASSIGN(DictationButtonTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_DICTATION_BUTTON_TRAY_H_
