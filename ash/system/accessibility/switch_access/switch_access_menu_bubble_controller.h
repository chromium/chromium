// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_

#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class SwitchAccessBackButtonBubbleController;
class SwitchAccessMenuView;

// Manages the Switch Access menu bubble.
class ASH_EXPORT SwitchAccessMenuBubbleController
    : public TrayBubbleView::Delegate {
 public:
  SwitchAccessMenuBubbleController();
  ~SwitchAccessMenuBubbleController() override;

  SwitchAccessMenuBubbleController(const SwitchAccessMenuBubbleController&) =
      delete;
  SwitchAccessMenuBubbleController& operator=(
      const SwitchAccessMenuBubbleController&) = delete;

  void ShowBackButton(const gfx::Rect& anchor);
  void HideBackButton();

  void ShowMenu(const gfx::Rect& anchor,
                const std::vector<std::string>& actions_to_show);
  void HideMenuBubble();

  // TrayBubbleView::Delegate:
  void BubbleViewDestroyed() override;

 private:
  friend class SwitchAccessBackButtonBubbleControllerTest;
  friend class SwitchAccessMenuBubbleControllerTest;

  void ShowBackButtonForMenu();

  std::unique_ptr<SwitchAccessBackButtonBubbleController>
      back_button_controller_;
  bool menu_open_ = false;

  // Owned by views hierarchy.
  raw_ptr<SwitchAccessMenuView, ExperimentalAsh> menu_view_ = nullptr;
  raw_ptr<TrayBubbleView, ExperimentalAsh> bubble_view_ = nullptr;

  raw_ptr<views::Widget, ExperimentalAsh> widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUBBLE_CONTROLLER_H_
