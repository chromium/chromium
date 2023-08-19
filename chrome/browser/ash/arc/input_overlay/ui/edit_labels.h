// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;
class EditLabel;
class NameTag;

// EditLabels wraps the input labels belonging to one action.
class EditLabels : public views::View {
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
      bool set_title);

  EditLabels(DisplayOverlayController* controller,
             Action* action,
             NameTag* name_tag,
             bool set_title);

  EditLabels(const EditLabels&) = delete;
  EditLabels& operator=(const EditLabels&) = delete;
  ~EditLabels() override;

  void OnActionInputBindingUpdated();

  void SetNameTagState(bool is_error, const std::u16string& error_tooltip);

 private:
  friend class EditLabelTest;

  void Init();
  void InitForActionTapKeyboard();
  void InitForActionMoveKeyboard();

  // Called when `labels_` is initiated or changes the content.
  void UpdateNameTag();
  // Called when the editing list is first loaded to assign name labels to
  // name tags, if available.
  void UpdateNameTagTitle();

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;
  // Displays the content in `labels_`.
  raw_ptr<NameTag, DanglingUntriaged> name_tag_ = nullptr;

  std::vector<EditLabel*> labels_;

  // It is true that at least one of `labels_` is unassigned.
  bool missing_assign_ = false;

  // Allows for title modification if true.
  bool set_title_ = false;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
