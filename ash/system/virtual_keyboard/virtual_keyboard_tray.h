// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
#define ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"
#include "base/macros.h"

namespace views {
class ImageView;
}

namespace ash {

// TODO(sky): make this visible on non-chromeos platforms.
class VirtualKeyboardTray : public TrayBackgroundView,
                            public AccessibilityObserver,
                            public KeyboardControllerObserver,
                            public ShellObserver,
                            public SessionObserver {
 public:
  explicit VirtualKeyboardTray(Shelf* shelf);
  ~VirtualKeyboardTray() override;

  // TrayBackgroundView:
  base::string16 GetAccessibleNameForTray() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // Updates the icon UI.
  void UpdateIcon();

  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  Shelf* shelf_;

  ScopedSessionObserver session_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(VirtualKeyboardTray);
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
