// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"

#include <algorithm>

#include "ash/style/icon_button.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_label.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_labels.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/name_tag.h"
#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"
#include "ui/views/view_utils.h"

namespace arc::input_overlay {

OverlayViewTestBase::OverlayViewTestBase() : GameControlsTestBase() {}

OverlayViewTestBase::~OverlayViewTestBase() = default;

void OverlayViewTestBase::EnableEditMode() {
  EnableDisplayMode(DisplayMode::kEdit);
  editing_list_ = controller_->GetEditingList();
  // Ensure editing_list_ show up in edit mode.
  ASSERT_TRUE(editing_list_);
}

void OverlayViewTestBase::PressAddButton() {
  if (!editing_list_) {
    return;
  }
  LeftClickOn(editing_list_->GetAddButtonForTesting());
}

void OverlayViewTestBase::PressAddContainerButton() {
  if (!editing_list_) {
    return;
  }
  LeftClickOn(editing_list_->GetAddContainerButtonForTesting());
}

void OverlayViewTestBase::PressDoneButton() {
  if (!editing_list_) {
    return;
  }
  LeftClickOn(editing_list_->done_button_);
}

void OverlayViewTestBase::AddNewActionInCenter() {
  DCHECK(editing_list_);

  PressAddButton();
  const auto* target_view = GetTargetView();
  DCHECK(target_view);
  LeftClickOn(target_view);
}

ButtonOptionsMenu* OverlayViewTestBase::ShowButtonOptionsMenu(Action* action) {
  action->action_view()->ShowButtonOptionsMenu();
  DCHECK(controller_->button_options_widget_);
  return controller_->GetButtonOptionsMenu();
}

void OverlayViewTestBase::PressDoneButtonOnButtonOptionsMenu() {
  DCHECK(controller_);
  if (auto* menu = controller_->GetButtonOptionsMenu()) {
    menu->OnDoneButtonPressed();
  }
}

void OverlayViewTestBase::PressDeleteButtonOnButtonOptionsMenu() {
  DCHECK(controller_);
  if (auto* menu = controller_->GetButtonOptionsMenu()) {
    LeftClickOn(menu->trash_button_);
  }
}

void OverlayViewTestBase::HoverAtActionViewListItem(size_t index) {
  if (auto* list_item = GetEditingListItem(index)) {
    GetEventGenerator()->MoveMouseTo(
        list_item->GetBoundsInScreen().CenterPoint());
  }
}

size_t OverlayViewTestBase::GetActionViewSize() const {
  if (!input_mapping_view_) {
    return 0;
  }

  return input_mapping_view_->children().size();
}

size_t OverlayViewTestBase::GetActionListItemsSize() const {
  if (auto* list = GetEditingList()) {
    return list->scroll_content_->children().size();
  }
  return 0;
}

ButtonOptionsMenu* OverlayViewTestBase::GetButtonOptionsMenu() const {
  DCHECK(controller_);
  return controller_->GetButtonOptionsMenu();
}

DeleteEditShortcut* OverlayViewTestBase::GetDeleteEditShortcut() const {
  DCHECK(controller_);
  return controller_->GetDeleteEditShortcut();
}

EditingList* OverlayViewTestBase::GetEditingList() const {
  DCHECK(controller_);
  return controller_->GetEditingList();
}

views::View* OverlayViewTestBase::GetEditingListItem(size_t index) const {
  if (auto* list = GetEditingList()) {
    if (auto& children = list->scroll_content_->children();
        index < children.size()) {
      return children[index];
    }
  }
  return nullptr;
}

TargetView* OverlayViewTestBase::GetTargetView() const {
  DCHECK(controller_);
  return controller_->GetTargetView();
}

EditLabel* OverlayViewTestBase::GetEditLabel(ActionViewListItem* list_item,
                                             size_t index) const {
  auto& labels = list_item->labels_view_->labels_;
  DCHECK_LT(index, labels.size());
  return labels[index];
}

EditLabel* OverlayViewTestBase::GetEditLabel(ButtonOptionsMenu* menu,
                                             size_t index) const {
  auto& labels = menu->action_edit_->labels_view_->labels_;
  DCHECK_LT(index, labels.size());
  return labels[index];
}

Action* OverlayViewTestBase::GetButtonOptionsMenuAction() const {
  if (auto* menu = controller_->GetButtonOptionsMenu()) {
    return menu->action();
  }
  return nullptr;
}

Action* OverlayViewTestBase::GetEditingListItemAction(size_t index) const {
  if (auto* list_item_view = GetEditingListItem(index)) {
    if (auto* list_item =
            views::AsViewClass<ActionViewListItem>(list_item_view)) {
      return list_item->action();
    }
  }
  return nullptr;
}

void OverlayViewTestBase::VerifyUIDisplay(
    Action* action,
    const std::vector<std::u16string>& expected_labels,
    const std::u16string& expected_name) const {
  DCHECK(action);
  VerifyActionView(action->action_view(), expected_labels);

  auto* const list_item = GetEditingListItem(action);
  EXPECT_TRUE(list_item);
  VerifyEditingListItem(list_item, expected_labels, expected_name);

  if (auto* menu = GetButtonOptionsMenu(); menu->action() == action) {
    VerifyButtonOptionsMenu(menu, expected_labels, expected_name);
  }
}

void OverlayViewTestBase::VerifyActionKeyBinding(
    Action* action,
    const std::vector<ui::DomCode>& expected_code) const {
  const auto& current_keys = action->current_input()->keys();
  const size_t size = current_keys.size();
  EXPECT_EQ(expected_code.size(), size);
  EXPECT_TRUE(std::equal(expected_code.begin(), expected_code.end(),
                         current_keys.begin()));
}

// Create a GIO enabled window with default actions including two action tap and
// one action move, enable it into edit mode.
void OverlayViewTestBase::SetUp() {
  GameControlsTestBase::SetUp();

  tap_action_ = touch_injector_->actions()[0].get();
  tap_action_two_ = touch_injector_->actions()[1].get();
  move_action_ = touch_injector_->actions()[2].get();

  input_mapping_view_ = controller_->GetInputMapping();
  DCHECK(input_mapping_view_);

  EnableEditMode();

  DCHECK(editing_list_->scroll_content_);
  const auto& items = editing_list_->scroll_content_->children();
  DCHECK_EQ(items.size(), 3u);
  tap_action_list_item_ = views::AsViewClass<ActionViewListItem>(items[0]);
  tap_action_list_item_two_ = views::AsViewClass<ActionViewListItem>(items[1]);
  move_action_list_item_ = views::AsViewClass<ActionViewListItem>(items[2]);
  DCHECK(tap_action_list_item_);
  DCHECK(move_action_list_item_);
}

ActionViewListItem* OverlayViewTestBase::GetEditingListItem(
    Action* action) const {
  return controller_->GetEditingListItemForAction(action);
}

void OverlayViewTestBase::VerifyButtonOptionsMenu(
    ButtonOptionsMenu* menu,
    const std::vector<std::u16string>& expected_labels,
    const std::u16string& expected_name) const {
  DCHECK(menu);
  const auto& labels = menu->action_edit_->labels_view_->labels_;
  const size_t size = labels.size();
  EXPECT_EQ(expected_labels.size(), size);
  EXPECT_TRUE(std::equal(expected_labels.begin(), expected_labels.end(),
                         labels.begin(),
                         [](std::u16string a, EditLabel* label) {
                           return a == label->GetText();
                         }));
  EXPECT_EQ(menu->action_name_label_->GetText(), expected_name);
}

void OverlayViewTestBase::VerifyEditingListItem(
    ActionViewListItem* list_item,
    const std::vector<std::u16string>& expected_labels,
    const std::u16string& expected_name) const {
  DCHECK(list_item);
  const auto& labels = list_item->labels_view_->labels_;
  const size_t size = labels.size();
  EXPECT_EQ(expected_labels.size(), size);
  EXPECT_TRUE(std::equal(expected_labels.begin(), expected_labels.end(),
                         labels.begin(),
                         [](std::u16string a, EditLabel* label) {
                           return a == label->GetText();
                         }));
  EXPECT_EQ(list_item->name_tag_->title_label_->GetText(), expected_name);
}

void OverlayViewTestBase::VerifyActionView(
    ActionView* action_view,
    const std::vector<std::u16string>& expected_labels) const {
  DCHECK(action_view);
  const auto* action = action_view->action();
  DCHECK(action);

  const auto& labels = action_view->labels();
  const size_t size = labels.size();
  EXPECT_EQ(expected_labels.size(), size);
  for (size_t i = 0; i < size; i++) {
    EXPECT_EQ(action->is_new()
                  ? (expected_labels[i].empty() ? u"?" : expected_labels[i])
                  : expected_labels[i],
              labels[i]->GetText());
  }
}

}  // namespace arc::input_overlay
