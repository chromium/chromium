// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include <memory>

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/lottie/resource.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class ButtonOptionsMenuTest : public OverlayViewTestBase {
 public:
  ButtonOptionsMenuTest() = default;
  ~ButtonOptionsMenuTest() override = default;

  size_t GetActionListItemsSize() {
    DCHECK(editing_list_);

    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    if (editing_list_->HasControls()) {
      return scroll_content->children().size();
    }
    return 0;
  }

  size_t GetActionViewSize() { return input_mapping_view_->children().size(); }

  bool IsEditingListInZeroState() { return editing_list_->is_zero_state_; }

  void PressTrashButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    menu->OnTrashButtonPressed();
  }

  ActionType GetActionType(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    return menu->action()->GetType();
  }

  void PressActionMoveButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionTypeButtonGroup* button_group = menu->button_group_;
    DCHECK(button_group);
    button_group->OnActionMoveButtonPressed();
  }

  void PressTapButton(ButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionTypeButtonGroup* button_group = menu->button_group_;
    DCHECK(button_group);
    button_group->OnActionTapButtonPressed();
  }

  bool IsActionInTouchInjector(Action* action) {
    const auto& actions = touch_injector_->actions();
    return std::find_if(actions.begin(), actions.end(),
                        [&](const std::unique_ptr<Action>& p) {
                          return action == p.get();
                        }) != actions.end();
  }

  bool IsActionInEditingList(Action* action) {
    views::View* scroll_content = editing_list_->scroll_content_;
    DCHECK(scroll_content);
    for (auto* child : scroll_content->children()) {
      auto* list_item = static_cast<ActionViewListItem*>(child);
      DCHECK(list_item);
      if (list_item->action() == action) {
        return true;
      }
    }
    return false;
  }

 private:
  void SetUp() override {
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);

    OverlayViewTestBase::SetUp();
  }
};

TEST_F(ButtonOptionsMenuTest, TestRemoveAction) {
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_EQ(3u, GetActionListItemsSize());
  EXPECT_EQ(3u, GetActionViewSize());
  EXPECT_FALSE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());

  // Remove Action Tap.
  auto* menu = ShowButtonOptionsMenu(tap_action_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_FALSE(move_action_->IsDeleted());
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());

  // Remove Action Move.
  menu = ShowButtonOptionsMenu(move_action_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_FALSE(tap_action_two_->IsDeleted());
  EXPECT_TRUE(move_action_->IsDeleted());
  EXPECT_FALSE(IsEditingListInZeroState());
  EXPECT_EQ(1u, GetActionViewSize());

  // Remove Action Move.
  menu = ShowButtonOptionsMenu(tap_action_two_);
  PressTrashButton(menu);
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_, /*expect_size=*/3u, /*expect_types=*/
               {ActionType::TAP, ActionType::TAP, ActionType::MOVE},
               /*expect_ids=*/{0, 1, 2});
  EXPECT_TRUE(tap_action_->IsDeleted());
  EXPECT_TRUE(tap_action_two_->IsDeleted());
  EXPECT_TRUE(move_action_->IsDeleted());
  EXPECT_TRUE(IsEditingListInZeroState());
  EXPECT_EQ(0u, GetActionViewSize());
}

TEST_F(ButtonOptionsMenuTest, TestChangeActionType) {
  // Change Action Tap.
  auto* menu = ShowButtonOptionsMenu(tap_action_);
  EXPECT_EQ(GetActionType(menu), ActionType::TAP);
  PressActionMoveButton(menu);
  EXPECT_EQ(GetActionType(menu), ActionType::MOVE);
  EXPECT_TRUE(IsActionInTouchInjector(menu->action()));
  EXPECT_TRUE(IsActionInEditingList(menu->action()));
  // Change Action Move.
  menu = ShowButtonOptionsMenu(move_action_);
  EXPECT_EQ(GetActionType(menu), ActionType::MOVE);
  PressTapButton(menu);
  EXPECT_EQ(GetActionType(menu), ActionType::TAP);
  EXPECT_TRUE(IsActionInTouchInjector(menu->action()));
  EXPECT_TRUE(IsActionInEditingList(menu->action()));
}

}  // namespace arc::input_overlay
