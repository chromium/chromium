// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/overlay_view_test_base.h"

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/touch_point.h"

namespace arc::input_overlay {

OverlayViewTestBase::OverlayViewTestBase() : GameControlsTestBase() {}

OverlayViewTestBase::~OverlayViewTestBase() = default;

void OverlayViewTestBase::EnableEditMode() {
  EnableDisplayMode(DisplayMode::kEdit);
}

ButtonOptionsMenu* OverlayViewTestBase::ShowButtonOptionsMenu(Action* action) {
  action->action_view()->ShowButtonOptionsMenu();
  DCHECK(controller_->button_options_widget_);
  return static_cast<ButtonOptionsMenu*>(
      controller_->button_options_widget_->GetContentsView());
}

// Create a GIO enabled window with default actions including two action tap and
// one action move, enable it into edit mode.
void OverlayViewTestBase::SetUp() {
  GameControlsTestBase::SetUp();
  EnableEditMode();

  tap_action_ = touch_injector_->actions()[0].get();
  tap_action_two_ = touch_injector_->actions()[1].get();
  move_action_ = touch_injector_->actions()[2].get();

  input_mapping_view_ = static_cast<InputMappingView*>(
      controller_->input_mapping_widget_->GetContentsView());

  DCHECK(controller_->editing_list_widget_);
  editing_list_ = static_cast<EditingList*>(
      controller_->editing_list_widget_->GetContentsView());
  DCHECK(editing_list_->scroll_content_);
  const auto& items = editing_list_->scroll_content_->children();
  DCHECK_EQ(items.size(), 3u);
  tap_action_list_item_ = static_cast<ActionViewListItem*>(items[0]);
  tap_action_list_item_two_ = static_cast<ActionViewListItem*>(items[1]);
  move_action_list_item_ = static_cast<ActionViewListItem*>(items[2]);
  DCHECK(tap_action_list_item_);
  DCHECK(move_action_list_item_);
}

}  // namespace arc::input_overlay
