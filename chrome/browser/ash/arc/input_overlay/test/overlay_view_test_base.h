// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/test/game_controls_test_base.h"

namespace arc::input_overlay {

class Action;
class ActionViewListItem;
class ButtonOptionsMenu;
class EditingList;
class InputMappingView;

// UI test base for beta+ version.
class OverlayViewTestBase : public GameControlsTestBase {
 public:
  OverlayViewTestBase();
  ~OverlayViewTestBase() override;

 protected:
  void EnableEditMode();
  void PressAddButton();

  size_t GetActionViewSize();

  ButtonOptionsMenu* ShowButtonOptionsMenu(Action* action);
  views::Widget* GetTargetViewWidget();

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
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_
