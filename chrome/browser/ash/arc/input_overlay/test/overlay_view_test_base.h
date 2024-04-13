// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/test/game_controls_test_base.h"

namespace arc::input_overlay {

class Action;
class ActionView;
class ActionViewListItem;
class ButtonOptionsMenu;
class DeleteEditShortcut;
class EditingList;
class EditLabel;
class InputMappingView;
class TargetView;

// UI test base for beta+ version.
class OverlayViewTestBase : public GameControlsTestBase {
 public:
  OverlayViewTestBase();
  ~OverlayViewTestBase() override;

 protected:
  void EnableEditMode();
  void PressAddButton();
  void PressAddContainerButton();
  void PressDoneButton();

  // Adds a new action in the center of the main window.
  void AddNewActionInCenter();

  ButtonOptionsMenu* ShowButtonOptionsMenu(Action* action);
  void PressDoneButtonOnButtonOptionsMenu();
  void PressDeleteButtonOnButtonOptionsMenu();
  void HoverAtActionViewListItem(size_t index);

  size_t GetActionViewSize() const;
  size_t GetActionListItemsSize() const;

  ButtonOptionsMenu* GetButtonOptionsMenu() const;
  DeleteEditShortcut* GetDeleteEditShortcut() const;
  EditingList* GetEditingList() const;
  views::View* GetEditingListItem(size_t index) const;
  TargetView* GetTargetView() const;

  EditLabel* GetEditLabel(ActionViewListItem* list_item, size_t index) const;
  EditLabel* GetEditLabel(ButtonOptionsMenu* menu, size_t index) const;

  Action* GetButtonOptionsMenuAction() const;
  Action* GetEditingListItemAction(size_t index) const;

  void VerifyUIDisplay(Action* action,
                       const std::vector<std::u16string>& expected_labels,
                       const std::u16string& expected_name) const;
  void VerifyActionKeyBinding(
      Action* action,
      const std::vector<ui::DomCode>& expected_code) const;

  // GameControlsTestBase:
  void SetUp() override;

  raw_ptr<Action, DanglingUntriaged> tap_action_;
  raw_ptr<Action, DanglingUntriaged> tap_action_two_;
  raw_ptr<Action, DanglingUntriaged> move_action_;

  raw_ptr<InputMappingView, DanglingUntriaged> input_mapping_view_;
  raw_ptr<EditingList, DanglingUntriaged> editing_list_;
  raw_ptr<ActionViewListItem, DanglingUntriaged> tap_action_list_item_;
  raw_ptr<ActionViewListItem, DanglingUntriaged> tap_action_list_item_two_;
  raw_ptr<ActionViewListItem, DanglingUntriaged> move_action_list_item_;

  gfx::Point local_location_;

 private:
  ActionViewListItem* GetEditingListItem(Action* action) const;

  // Verifies UI display.
  void VerifyButtonOptionsMenu(
      ButtonOptionsMenu* menu,
      const std::vector<std::u16string>& expected_labels,
      const std::u16string& expected_name) const;
  void VerifyEditingListItem(ActionViewListItem* list_item,
                             const std::vector<std::u16string>& expected_labels,
                             const std::u16string& expected_name) const;
  void VerifyActionView(
      ActionView* action_view,
      const std::vector<std::u16string>& expected_labels) const;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_
