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

}  // namespace arc::input_overlay
