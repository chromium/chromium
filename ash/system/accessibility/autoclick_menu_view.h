// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace views {
class Button;
class Separator;
}  // namespace views

namespace ash {

class FloatingMenuButton;

// View for the Automatic Clicks Menu, which creates and manages
// individual buttons to control Automatic Clicks settings.
class AutoclickMenuView : public views::BoxLayoutView {
  METADATA_HEADER(AutoclickMenuView, views::BoxLayoutView)

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
  AutoclickMenuView(const AutoclickMenuView&) = delete;
  AutoclickMenuView& operator=(const AutoclickMenuView&) = delete;
  ~AutoclickMenuView() override = default;

  void UpdateEventType(AutoclickEventType type);
  void UpdatePosition(FloatingMenuPosition position);

 private:
  void OnAutoclickButtonPressed(views::Button* sender);
  void OnPositionButtonPressed();

  // Unowned. Owned by views hierarchy.
  raw_ptr<FloatingMenuButton> left_click_button_ = nullptr;
  raw_ptr<FloatingMenuButton> right_click_button_ = nullptr;
  raw_ptr<FloatingMenuButton> double_click_button_ = nullptr;
  raw_ptr<FloatingMenuButton> drag_button_ = nullptr;
  raw_ptr<FloatingMenuButton> scroll_button_ = nullptr;
  raw_ptr<FloatingMenuButton> pause_button_ = nullptr;
  raw_ptr<FloatingMenuButton> position_button_ = nullptr;
  raw_ptr<views::Separator> separator_ = nullptr;

  // The most recently selected event_type_ excluding kNoAction. This is used
  // when the pause button is selected in order to unpause and reset to the
  // previous state.
  AutoclickEventType event_type_ = AutoclickEventType::kLeftClick;
};

BEGIN_VIEW_BUILDER(/* no export */, AutoclickMenuView, views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::AutoclickMenuView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_AUTOCLICK_MENU_VIEW_H_
