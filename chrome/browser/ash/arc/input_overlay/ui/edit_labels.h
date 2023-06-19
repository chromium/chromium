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
      Action* action);

  EditLabels(DisplayOverlayController* controller, Action* action);

  EditLabels(const EditLabels&) = delete;
  EditLabels& operator=(const EditLabels&) = delete;
  ~EditLabels() override;

  void OnActionUpdated();

  std::u16string GetTextForNameTag();

 private:
  friend class EditLabelTest;

  void Init();
  void InitForActionTapKeyboard();
  void InitForActionMoveKeyboard();

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;

  std::vector<EditLabel*> labels_;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
