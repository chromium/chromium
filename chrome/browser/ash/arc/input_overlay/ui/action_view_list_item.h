// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;

// ActionViewListItem shows in EditingList and is associated with each of
// Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------

class ActionViewListItem : public views::View {
 public:
  ActionViewListItem(DisplayOverlayController* controller, Action* action);
  ActionViewListItem(const ActionViewListItem&) = delete;
  ActionViewListItem& operator=(const ActionViewListItem&) = delete;
  ~ActionViewListItem() override;

 private:
  void Init();

  // Set list item of different types.
  void SetActionTapListItem(views::View* container);
  void SetActionMoveListItem(views::View* container);

  raw_ptr<DisplayOverlayController> controller_;
  raw_ptr<Action> action_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
