// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"

namespace arc::input_overlay {

class EditingListTest : public OverlayViewTestBase {
 public:
  EditingListTest() = default;
  ~EditingListTest() override = default;

  size_t GetActionListItemsSize() {
    DCHECK(editing_list_->scroll_content_);
    DCHECK(editing_list_);
    if (editing_list_->HasControls()) {
      return editing_list_->scroll_content_->children().size();
    }
    return 0;
  }

  size_t GetActionViewSize() {
    DCHECK(input_mapping_view_);
    return input_mapping_view_->children().size();
  }

  size_t GetTouchInjectorActionSize() {
    DCHECK(touch_injector_);
    return touch_injector_->actions().size();
  }

  void PressAddButton() {
    DCHECK(editing_list_);
    editing_list_->OnAddButtonPressed();
  }

  void PressLeftMouseAtEditingList() {
    // Press down at the center of the editing list.
    local_location_ = editing_list_->bounds().CenterPoint();
    local_location_.set_y(editing_list_->bounds().y() + 10);
    auto press =
        ui::MouseEvent(ui::ET_MOUSE_PRESSED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    editing_list_->OnMousePressed(press);
  }

  void MouseDragEditingListBy(const gfx::Vector2d& delta_move) {
    local_location_ += delta_move;
    auto drag =
        ui::MouseEvent(ui::ET_MOUSE_DRAGGED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
    editing_list_->OnMouseDragged(drag);
  }

  void ReleaseLeftMouse() {
    auto release =
        ui::MouseEvent(ui::ET_MOUSE_RELEASED, local_location_, local_location_,
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                       ui::EF_LEFT_MOUSE_BUTTON);
    editing_list_->OnMouseReleased(release);
  }

  void TouchPressAtEditingList() {
    // Press down at the center of the editing list.
    local_location_ = editing_list_->bounds().CenterPoint();
    // Offset to the rough location of the "Editing" text block.
    local_location_.set_y(editing_list_->bounds().y() + 10);

    auto scroll_begin =
        ui::GestureEvent(local_location_.x(), local_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN,
                                                 /*delta_x=*/0, /*delta_y=*/0));
    editing_list_->OnGestureEvent(&scroll_begin);
  }

  void TouchMoveEditingListBy(const gfx::Vector2d& delta_move) {
    local_location_ += delta_move;
    auto scroll_update = ui::GestureEvent(
        local_location_.x(), local_location_.y(), ui::EF_NONE,
        base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, delta_move.x(),
                                delta_move.y()));
    editing_list_->OnGestureEvent(&scroll_update);
  }

  void TouchReleaseAtEditingList() {
    auto scroll_end =
        ui::GestureEvent(local_location_.x(), local_location_.y(), ui::EF_NONE,
                         base::TimeTicks::Now(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
    editing_list_->OnGestureEvent(&scroll_end);
  }

  gfx::Point GetEditingListOrigin() { return editing_list_->origin(); }
};

TEST_F(EditingListTest, TestEditingListAddNewAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_EQ(3u, GetTouchInjectorActionSize());
  // Add a new action by pressing add button.
  PressAddButton();
  CheckActions(
      touch_injector_, /*expect_size=*/4u, /*expect_types=*/
      {ActionType::TAP, ActionType::TAP, ActionType::MOVE, ActionType::TAP},
      /*expect_ids=*/{0, 1, 2, kMaxDefaultActionID + 1});
  EXPECT_EQ(4u, GetActionListItemsSize());
  EXPECT_EQ(4u, GetActionViewSize());
  EXPECT_EQ(4u, GetTouchInjectorActionSize());
}

TEST_F(EditingListTest, TestEditingListReposition) {
  // Drag move by mouse.
  auto updated_pos = GetEditingListOrigin();
  PressLeftMouseAtEditingList();
  auto origin_mouse_pos = local_location_;
  MouseDragEditingListBy(gfx::Vector2d(50, 60));
  auto mouse_moved_delta = local_location_ - origin_mouse_pos;
  updated_pos += mouse_moved_delta;
  auto final_pos = GetEditingListOrigin();
  ReleaseLeftMouse();
  // With the magnetic feature, the final position should
  // be different than the final dragged position.
  EXPECT_EQ(updated_pos, final_pos);
  EXPECT_NE(updated_pos, GetEditingListOrigin());

  // Drag move by touch.
  updated_pos = GetEditingListOrigin();
  TouchPressAtEditingList();
  origin_mouse_pos = local_location_;
  TouchMoveEditingListBy(gfx::Vector2d(60, 100));
  mouse_moved_delta = local_location_ - origin_mouse_pos;
  updated_pos += mouse_moved_delta;
  final_pos = GetEditingListOrigin();
  TouchReleaseAtEditingList();
  // With the magnetic feature, the final position should
  // be different than the final dragged position.
  EXPECT_EQ(updated_pos, final_pos);
  EXPECT_NE(updated_pos, GetEditingListOrigin());
}

}  // namespace arc::input_overlay
