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
      bool for_editing_list);

  EditLabels(DisplayOverlayController* controller,
             Action* action,
             NameTag* name_tag,
             bool for_editing_list);

  EditLabels(const EditLabels&) = delete;
  EditLabels& operator=(const EditLabels&) = delete;
  ~EditLabels() override;

  void OnActionInputBindingUpdated();

  void SetNameTagState(bool is_error, const std::u16string& error_tooltip);
  void RemoveNewState();

  // Focuses on the first edit label if no child is focused. Otherwise focus on
  // the next edit label.
  void FocusLabel();

  // Returns Action name, such as "Joystick wasd".
  std::u16string CalculateActionName();
  // Returns key list, such as "w, a, s, d" or "w".
  std::u16string CalculateKeyListForA11yLabel() const;

  bool IsFirstLabelUnassigned() const;

  void PerformPulseAnimationOnFirstLabel();

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;
  friend class OverlayViewTestBase;

  void Init();
  void InitForActionTapKeyboard();
  void InitForActionMoveKeyboard();

  // Called when `labels_` is initiated or changes the content.
  void UpdateNameTag();

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;
  // Displays the content in `labels_`.
  raw_ptr<NameTag, DanglingUntriaged> name_tag_ = nullptr;
  const bool for_editing_list_ = false;

  std::vector<raw_ptr<EditLabel, VectorExperimental>> labels_;

  // It is true that at least one of `labels_` is unassigned.
  bool missing_assign_ = false;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABELS_H_
