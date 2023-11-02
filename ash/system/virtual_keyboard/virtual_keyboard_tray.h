// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
#define ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_background_view.h"

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
  VirtualKeyboardTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);

  VirtualKeyboardTray(const VirtualKeyboardTray&) = delete;
  VirtualKeyboardTray& operator=(const VirtualKeyboardTray&) = delete;

  ~VirtualKeyboardTray() override;

  // TrayBackgroundView:
  void Initialize() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;
  bool PerformAction(const ui::Event& event) override;
  void OnThemeChanged() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // Weak pointer, will be parented by TrayContainer for its lifetime.
  views::ImageView* icon_;

  Shelf* shelf_;

  ScopedSessionObserver session_observer_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
