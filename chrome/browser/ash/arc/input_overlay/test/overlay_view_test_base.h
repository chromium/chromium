// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_OVERLAY_VIEW_TEST_BASE_H_

#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"

namespace arc::input_overlay {

class Action;
class ActionViewListItem;
class ArcInputOverlayManager;
class ButtonOptionsMenu;
class DisplayOverlayController;
class EditingList;
class InputMappingView;
class TouchInjector;

// UI test base for beta+ version.
class OverlayViewTestBase : public ash::AshTestBase {
 public:
  OverlayViewTestBase();
  ~OverlayViewTestBase() override;

 protected:
  TouchInjector* GetTouchInjector(aura::Window* window);
  DisplayOverlayController* GetDisplayOverlayController();
  void EnableEditMode();
  ButtonOptionsMenu* ShowButtonOptionsMenu(Action* action);

  // ash::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  std::unique_ptr<ArcInputOverlayManager> arc_test_input_overlay_manager_;
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<views::Widget> widget_;
  raw_ptr<TouchInjector, DanglingUntriaged> touch_injector_;
  raw_ptr<DisplayOverlayController, DanglingUntriaged> controller_;
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
