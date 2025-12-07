// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUTTON_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUTTON_H_

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/metadata/view_factory.h"

namespace gfx {
struct VectorIcon;
}

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

class SwitchAccessMenuButton : public views::Button {
  METADATA_HEADER(SwitchAccessMenuButton, views::Button)

 public:
  SwitchAccessMenuButton(std::string action_name,
                         const gfx::VectorIcon& icon,
                         int accessible_name_id);
  ~SwitchAccessMenuButton() override = default;

  SwitchAccessMenuButton(const SwitchAccessMenuButton&) = delete;
  SwitchAccessMenuButton& operator=(const SwitchAccessMenuButton&) = delete;

  static constexpr int kWidthDip = 80;

 private:
  friend class SwitchAccessMenuBubbleControllerTest;

  void OnButtonPressed();

  // Owned by the views hierarchy.
  raw_ptr<views::ImageView> image_view_;
  raw_ptr<views::Label> label_;
};

BEGIN_VIEW_BUILDER(/* no export */, SwitchAccessMenuButton, views::Button)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::SwitchAccessMenuButton)

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_SWITCH_ACCESS_MENU_BUTTON_H_
