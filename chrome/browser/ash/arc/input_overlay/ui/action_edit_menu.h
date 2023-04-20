// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

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

  static std::unique_ptr<ActionEditMenu> BuildActionEditMenu(
      DisplayOverlayController* display_overlay_controller,
      ActionView* anchor,
      ActionType action_type);

 private:
  class BindingButton;

  // Create edit menu for each action types.
  void InitActionTapEditMenu();
  void InitActionMoveEditMenu();

  // Function calls for each menu item button.
  void OnKeyBoardKeyBindingButtonPressed();
  void OnMouseLeftClickBindingButtonPressed();
  void OnMouseRightClickBindingButtonPressed();
  void OnResetButtonPressed();

  // Reference to owner class.
  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;
  // Reference to position.
  raw_ptr<ActionView> anchor_view_ = nullptr;
  // Reference to the menu items.
  raw_ptr<BindingButton> keyboard_key_ = nullptr;
  raw_ptr<BindingButton> mouse_left_ = nullptr;
  raw_ptr<BindingButton> mouse_right_ = nullptr;
  raw_ptr<BindingButton> reset_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_MENU_H_
