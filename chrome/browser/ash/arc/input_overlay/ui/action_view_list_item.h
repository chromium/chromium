// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_

#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class EditLabels;
class NameTag;

// ActionViewListItem shows in EditingList and is associated with each of
// Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------

class ActionViewListItem : public views::Button {
 public:
  ActionViewListItem(DisplayOverlayController* controller, Action* action);
  ActionViewListItem(const ActionViewListItem&) = delete;
  ActionViewListItem& operator=(const ActionViewListItem&) = delete;
  ~ActionViewListItem() override;

  void OnActionInputBindingUpdated();
  void OnActionNameUpdated();

  Action* action() const { return action_; }

 private:
  friend class EditLabelTest;

  void Init();

  void ShowButtonOptionsMenu();

  raw_ptr<DisplayOverlayController> controller_;
  raw_ptr<Action, DanglingUntriaged> action_;

  raw_ptr<EditLabels> labels_view_ = nullptr;
  raw_ptr<NameTag> name_tag_ = nullptr;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_VIEW_LIST_ITEM_H_
