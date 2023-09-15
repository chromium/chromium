// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_

#include "base/memory/raw_ptr.h"

#include "ash/style/rounded_container.h"
#include "ui/views/controls/button/button.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class EditLabels;
class NameTag;

// ActionEditView is associated with each of Action.
// ----------------------------
// | |Name tag|        |keys| |
// ----------------------------
class ActionEditView : public views::Button {
 public:
  ActionEditView(DisplayOverlayController* controller,
                 Action* action,
                 ash::RoundedContainer::Behavior container_type);
  ActionEditView(const ActionEditView&) = delete;
  ActionEditView& operator=(const ActionEditView&) = delete;
  ~ActionEditView() override;

  void RemoveNewState();

  virtual void OnActionNameUpdated();
  virtual void OnActionInputBindingUpdated();

  Action* action() const { return action_; }

 protected:
  virtual void ClickCallback() = 0;

  raw_ptr<DisplayOverlayController> controller_;
  raw_ptr<Action, DanglingUntriaged> action_;

  raw_ptr<EditLabels> labels_view_ = nullptr;
  raw_ptr<NameTag> name_tag_ = nullptr;

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  void OnClicked();
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_
