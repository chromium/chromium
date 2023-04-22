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
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageView;
}

namespace ash {

// TODO(sky): make this visible on non-chromeos platforms.
class VirtualKeyboardTray : public TrayBackgroundView,
                            public AccessibilityObserver,
                            public KeyboardControllerObserver,
                            public ShellObserver {
 public:
  METADATA_HEADER(VirtualKeyboardTray);

  VirtualKeyboardTray(Shelf* shelf, TrayBackgroundViewCatalogName catalog_name);
  VirtualKeyboardTray(const VirtualKeyboardTray&) = delete;
  VirtualKeyboardTray& operator=(const VirtualKeyboardTray&) = delete;
  ~VirtualKeyboardTray() override;

  // Callback called when this is pressed.
  void OnButtonPressed(const ui::Event& event);

  // TrayBackgroundView:
  void Initialize() override;
  std::u16string GetAccessibleNameForTray() override;
  void HandleLocaleChange() override;
  void HideBubbleWithView(const TrayBubbleView* bubble_view) override;
  void ClickedOutsideBubble() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool is_visible) override;

 private:
  // Owned by the views hierarchy.
  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;

  // Unowned.
  const raw_ptr<Shelf, ExperimentalAsh> shelf_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_VIRTUAL_KEYBOARD_VIRTUAL_KEYBOARD_TRAY_H_
