// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class EditLabel;
class NameTag;

// EditLabels wraps the input labels belonging to one action.
class EditLabels : public views::View {
  METADATA_HEADER(EditLabels, views::View)

 public:
  // Create key layout view depending on action type.
  // ActionTap for keyboard binding:
  //    -----
  //    ||a||
  //    -----
  //
  // ActionMove for keyboard binding:
  // -------------
  // |   | w |   |
  // |-----------|
  // | a | s | d |
  // -------------
  static std::unique_ptr<EditLabels> CreateEditLabels(
      DisplayOverlayController* controller,
      Action* action,
      NameTag* name_tag,
      bool should_update_title);

  EditLabels(DisplayOverlayController* controller,
             Action* action,
             NameTag* name_tag,
             bool should_update_title);

  EditLabels(const EditLabels&) = delete;
  EditLabels& operator=(const EditLabels&) = delete;
  ~EditLabels() override;

  void OnActionInputBindingUpdated();

  void SetNameTagState(bool is_error, const std::u16string& error_tooltip);
  void RemoveNewState();
  // Called when this view is clicked upon.
  void FocusLabel();

  void ShowEduNudgeForEditingTip();

  // Returns Action name, such as "Joystick WASD".
  std::u16string CalculateActionName();

  void set_should_update_title(bool should_update_title) {
    should_update_title_ = should_update_title;
  }

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  void Init();
  void InitForActionTapKeyboard();
  void InitForActionMoveKeyboard();

  // Called when `labels_` is initiated or changes the content.
  void UpdateNameTag();

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;
  // Displays the content in `labels_`.
  raw_ptr<NameTag, DanglingUntriaged> name_tag_ = nullptr;

  std::vector<EditLabel*> labels_;

  // It is true that at least one of `labels_` is unassigned.
  bool missing_assign_ = false;

  // Allows for title modification if true.
  bool should_update_title_ = false;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
