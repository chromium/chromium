// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class FloatingMenuButton;

// View for the Automatic Clicks Menu, which creates and manages
// individual buttons to control Automatic Clicks settings.
class AutoclickMenuView : public views::View, public views::ButtonListener {
 public:
  // Used for testing. Start at 1 because a view IDs should not be 0.
  enum class ButtonId {
    kPosition = 1,
    kLeftClick = 2,
    kRightClick = 3,
    kDoubleClick = 4,
    kDragAndDrop = 5,
    kScroll = 6,
    kPause = 7,
  };

  AutoclickMenuView(AutoclickEventType type, FloatingMenuPosition position);
  ~AutoclickMenuView() override = default;

  void UpdateEventType(AutoclickEventType type);
  void UpdatePosition(FloatingMenuPosition position);

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;

 private:
  // Unowned. Owned by views hierarchy.
  FloatingMenuButton* left_click_button_;
  FloatingMenuButton* right_click_button_;
  FloatingMenuButton* double_click_button_;
  FloatingMenuButton* drag_button_;
  FloatingMenuButton* scroll_button_ = nullptr;
  FloatingMenuButton* pause_button_;
  FloatingMenuButton* position_button_;

  // The most recently selected event_type_ excluding kNoAction. This is used
  // when the pause button is selected in order to unpause and reset to the
  // previous state.
  AutoclickEventType event_type_ = AutoclickEventType::kLeftClick;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
