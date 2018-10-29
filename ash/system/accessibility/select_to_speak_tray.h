// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"
#include "ui/views/controls/image_view.h"

namespace views {
class ImageView;
}

namespace ash {

// A button in the tray that lets users start/stop Select-to-Speak.
class ASH_EXPORT SelectToSpeakTray : public TrayBackgroundView,
                                     public AccessibilityObserver,
                                     public SessionObserver {
 public:
  explicit SelectToSpeakTray(Shelf* shelf);
  ~SelectToSpeakTray() override;

  // TrayBackgroundView:
  base::string16 GetAccessibleNameForTray() override;
  const char* GetClassName() const override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override {}
  void ClickedOutsideBubble() override {}
  bool PerformAction(const ui::Event& event) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

 private:
  friend class SelectToSpeakTrayTest;

  // Updates the icons color depending on if the user is logged-in or not.
  void UpdateIconsForSession();

  // Sets the icon when select-to-speak is activated (speaking) / deactivated.
  // Also updates visibility when select-to-speak is enabled / disabled.
  void CheckStatusAndUpdateIcon();

  gfx::ImageSkia inactive_image_;
  gfx::ImageSkia selecting_image_;
  gfx::ImageSkia speaking_image_;

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(SelectToSpeakTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_H_
