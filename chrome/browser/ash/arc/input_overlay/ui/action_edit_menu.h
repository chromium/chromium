// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_

#include <memory>

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace arc {
namespace input_overlay {

class DisplayOverlayController;
class ActionView;

// ActionEditMenu shows the rebinding options for each action. Currently, it
// only supports for tap action.
class ActionEditMenu : public views::View {
 public:
  ActionEditMenu(DisplayOverlayController* display_overlay_controller,
                 ActionView* anchor);
  ActionEditMenu(const ActionEditMenu&) = delete;
  ActionEditMenu& operator=(const ActionEditMenu&) = delete;
  ~ActionEditMenu() override;

  static std::unique_ptr<ActionEditMenu> BuildActionTapEditMenu(
      DisplayOverlayController* display_overlay_controller,
      ActionView* anchor);

 private:
  class BindingButton;

  void InitActionTapEditMenu();
  void OnKeyBoardKeyBindingButtonPressed();
  void OnMouseLeftBindingButtonPressed();
  void OnMouseRightBindingButtonPressed();
  void OnResetButtonPressed();

  // Reference to owner class.
  DisplayOverlayController* const display_overlay_controller_ = nullptr;
  // Reference to position.
  ActionView* anchor_ = nullptr;
  // Reference to the menu items.
  BindingButton* keyboard_key_ = nullptr;
  BindingButton* mouse_left_ = nullptr;
  BindingButton* mouse_right_ = nullptr;
  BindingButton* reset_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_
