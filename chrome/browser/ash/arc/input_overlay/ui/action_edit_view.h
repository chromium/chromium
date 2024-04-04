// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
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
  METADATA_HEADER(ActionEditView, views::Button)

 public:
  ActionEditView(DisplayOverlayController* controller,
                 Action* action,
                 bool for_editing_list);
  ActionEditView(const ActionEditView&) = delete;
  ActionEditView& operator=(const ActionEditView&) = delete;
  ~ActionEditView() override;

  virtual void OnActionInputBindingUpdated();

  void RemoveNewState();

  // Returns Action name, such as "Joystick WASD".
  std::u16string CalculateActionName();

  void PerformPulseAnimation();

  Action* action() const { return action_; }

 protected:
  virtual void ClickCallback() = 0;

  raw_ptr<DisplayOverlayController> controller_;
  raw_ptr<Action, DanglingUntriaged> action_;
  const bool for_editing_list_;

  raw_ptr<EditLabels> labels_view_ = nullptr;
  raw_ptr<NameTag> name_tag_ = nullptr;

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  void OnClicked();
  // Returns:
  // - "Selected key is w. Tap on the button to edit the control" or
  //   "Selected keys are w, a, s, d. Tap on the button to edit the control"
  //   if this view is on `EditingList`.
  // - "Tap on the button to focus on the label" if this view is on
  //   `ButtonOptionsMenu`.
  std::u16string CalculateAccessibleLabel() const;

  // views::View:
  void OnThemeChanged() override;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_EDIT_VIEW_H_
