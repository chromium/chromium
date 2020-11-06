// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_

#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class FloatingMenuButton;

// View for the Switch Access Back Button.
class SwitchAccessBackButtonView : public views::View,
                                   public views::ButtonListener {
 public:
  explicit SwitchAccessBackButtonView(bool for_menu);
  ~SwitchAccessBackButtonView() override = default;

  SwitchAccessBackButtonView(const SwitchAccessBackButtonView&) = delete;
  SwitchAccessBackButtonView& operator=(const SwitchAccessBackButtonView&) =
      delete;

  void SetFocusRing(bool should_show);
  void SetForMenu(bool for_menu);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  int GetHeightForWidth(int w) const override;
  const char* GetClassName() const override;
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  bool show_focus_ring_ = false;

  // Owned by views hierarchy.
  FloatingMenuButton* back_button_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SWITCH_ACCESS_BACK_BUTTON_VIEW_H_
