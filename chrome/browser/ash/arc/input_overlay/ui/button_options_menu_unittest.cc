// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/test/view_test_base.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_type_button_group.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/lottie/resource.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

namespace {

class TestButtonOptionsMenu : public ButtonOptionsMenu {
 public:
  TestButtonOptionsMenu(DisplayOverlayController* controller, Action* action)
      : ButtonOptionsMenu(controller, action) {}
  ~TestButtonOptionsMenu() override = default;

 private:
  // ButtonOptionsMenu:
  void CalculatePosition() override {}
};

}  // namespace

class ButtonOptionsMenuTest : public ViewTestBase {
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

  size_t GetActionViewSize() {
    DCHECK(input_mapping_view_);
    return input_mapping_view_->children().size();
  }

  bool IsEditingListInZeroState() { return editing_list_->is_zero_state_; }

  void ShowButtonOptionsMenu(ActionType action_type) {
    switch (action_type) {
      case ActionType::TAP:
        tap_action_menu_.reset();
        tap_action_menu_ = std::make_unique<TestButtonOptionsMenu>(
            display_overlay_controller_.get(), tap_action_);
        tap_action_menu_->Init();
        break;
      case ActionType::MOVE:
        move_action_menu_.reset();
        move_action_menu_ = std::make_unique<TestButtonOptionsMenu>(
            display_overlay_controller_.get(), move_action_);
        move_action_menu_->Init();
        break;
      default:
        NOTREACHED();
    }
  }

  void PressTrashButton(TestButtonOptionsMenu* menu) {
    DCHECK(menu);
    menu->OnTrashButtonPressed();
  }

  ActionType GetActionType(TestButtonOptionsMenu* menu) {
    DCHECK(menu);
    return menu->action()->GetType();
  }

  void PressActionMoveButton(TestButtonOptionsMenu* menu) {
    DCHECK(menu);
    ActionTypeButtonGroup* button_group = menu->button_group_;
    DCHECK(button_group);
    button_group->OnActionMoveButtonPressed();
  }

  void PressTapButton(TestButtonOptionsMenu* menu) {
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

  std::unique_ptr<EditingList> editing_list_;
  std::unique_ptr<TestButtonOptionsMenu> tap_action_menu_;
  std::unique_ptr<TestButtonOptionsMenu> move_action_menu_;

 private:
  void SetUp() override {
    ui::ResourceBundle::SetLottieParsingFunctions(
        &lottie::ParseLottieAsStillImage,
        &lottie::ParseLottieAsThemedStillImage);

    ViewTestBase::SetUp();
    InitWithFeature(ash::features::kArcInputOverlayBeta);
    SetDisplayMode(DisplayMode::kEdit);

    editing_list_ =
        std::make_unique<EditingList>(display_overlay_controller_.get());
    editing_list_->Init();
    DCHECK(editing_list_->scroll_content_);
  }

  void TearDown() override {
    editing_list_.reset();
    ViewTestBase::TearDown();
  }
};

TEST_F(ButtonOptionsMenuTest, TestRemoveAction) {
  CheckActions(touch_injector_.get(), /*expect_size=*/2u, /*expect_types=*/
               {ActionType::TAP, ActionType::MOVE}, /*expect_ids=*/{1, 0});
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());
  EXPECT_FALSE(touch_injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(touch_injector_->actions()[1]->IsDeleted());

  // Remove Action Tap.
  ShowButtonOptionsMenu(ActionType::TAP);
  PressTrashButton(tap_action_menu_.get());
  tap_action_menu_.reset();
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_.get(), /*expect_size=*/2u, /*expect_types=*/
               {ActionType::TAP, ActionType::MOVE}, /*expect_ids=*/{1, 0});
  EXPECT_TRUE(touch_injector_->actions()[0]->IsDeleted());
  EXPECT_FALSE(touch_injector_->actions()[1]->IsDeleted());
  EXPECT_EQ(1u, GetActionListItemsSize());
  EXPECT_EQ(1u, GetActionViewSize());

  // Remove Action Move.
  ShowButtonOptionsMenu(ActionType::MOVE);
  PressTrashButton(move_action_menu_.get());
  move_action_menu_.reset();
  // Default action is still in the list even it is deleted and it is marked as
  // deleted. But it doesn't show up visually.
  CheckActions(touch_injector_.get(), /*expect_size=*/2u, /*expect_types=*/
               {ActionType::TAP, ActionType::MOVE}, /*expect_ids=*/{1, 0});
  EXPECT_TRUE(touch_injector_->actions()[0]->IsDeleted());
  EXPECT_TRUE(touch_injector_->actions()[1]->IsDeleted());
  EXPECT_TRUE(IsEditingListInZeroState());
  EXPECT_EQ(0u, GetActionViewSize());
}

TEST_F(ButtonOptionsMenuTest, TestChangeActionType) {
  // Change Action Tap.
  ShowButtonOptionsMenu(ActionType::TAP);
  PressActionMoveButton(tap_action_menu_.get());
  EXPECT_EQ(GetActionType(tap_action_menu_.get()), ActionType::MOVE);
  EXPECT_TRUE(IsActionInTouchInjector(tap_action_menu_->action()));
  EXPECT_TRUE(IsActionInEditingList(tap_action_menu_->action()));
  tap_action_menu_.reset();
  // Change Action Move.
  ShowButtonOptionsMenu(ActionType::MOVE);
  PressTapButton(move_action_menu_.get());
  EXPECT_EQ(GetActionType(move_action_menu_.get()), ActionType::TAP);
  EXPECT_TRUE(IsActionInTouchInjector(move_action_menu_->action()));
  EXPECT_TRUE(IsActionInEditingList(move_action_menu_->action()));
  move_action_menu_.reset();
}

}  // namespace arc::input_overlay
