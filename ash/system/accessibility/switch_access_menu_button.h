// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUTTON_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUTTON_H_

#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class SwitchAccessMenuButton : public views::Button,
                               public views::ButtonListener {
 public:
  SwitchAccessMenuButton(std::string action_name,
                         const gfx::VectorIcon& icon,
                         int accessible_name_id);
  ~SwitchAccessMenuButton() override = default;

  SwitchAccessMenuButton(const SwitchAccessMenuButton&) = delete;
  SwitchAccessMenuButton& operator=(const SwitchAccessMenuButton&) = delete;

  static constexpr int kWidthDip = 80;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  friend class SwitchAccessMenuBubbleControllerTest;

  std::string action_name_;

  // Owned by the views hierarchy.
  views::ImageView* image_view_;
  views::Label* label_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_MENU_BUTTON_H_
