// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "chrome/browser/ash/arc/input_overlay/test/view_test_base.h"
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
  EXPECT_EQ(2u, GetActionListItemsSize());
  EXPECT_EQ(2u, GetActionViewSize());
  // Remove Action Tap.
  ShowButtonOptionsMenu(ActionType::TAP);
  PressTrashButton(tap_action_menu_.get());
  tap_action_menu_.reset();
  EXPECT_EQ(1u, GetActionListItemsSize());
  EXPECT_EQ(1u, GetActionViewSize());
  // Remove Action Move.
  ShowButtonOptionsMenu(ActionType::MOVE);
  PressTrashButton(move_action_menu_.get());
  move_action_menu_.reset();
  EXPECT_TRUE(IsEditingListInZeroState());
  EXPECT_EQ(0u, GetActionViewSize());
}

}  // namespace arc::input_overlay
