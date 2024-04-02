// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace arc::input_overlay {

class Action;
class DisplayOverlayController;

// EditLabel shows input mappings and can be edited to change mappings.
class EditLabel : public views::LabelButton {
  METADATA_HEADER(EditLabel, views::LabelButton)

 public:
  EditLabel(DisplayOverlayController* controller,
            Action* action,
            bool for_editing_list,
            size_t index = 0);

  EditLabel(const EditLabel&) = delete;
  EditLabel& operator=(const EditLabel&) = delete;
  ~EditLabel() override;

  void OnActionInputBindingUpdated();
  // Returns true if the EditLabel shows "?".
  bool IsInputUnbound();
  void RemoveNewState();

  void PerformPulseAnimation(int pulse_count);

 private:
  friend class ButtonOptionsMenuTest;
  friend class EditLabelTest;

  void Init();
  // Set label content depends on whether the label is in new state.
  void SetLabelContent();
  void SetTextLabel(const std::u16string& text);
  void SetNameTagState(bool is_error, const std::u16string& error_tooltip);
  void UpdateAccessibleName();
  void ChangeFocusToNextLabel();

  void SetToDefault();
  void SetToFocused();

  // views::View:
  void OnFocus() override;
  void OnBlur() override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  raw_ptr<DisplayOverlayController> controller_ = nullptr;
  raw_ptr<Action, DanglingUntriaged> action_ = nullptr;
  // A11y label is different for `EditingList` and `ButtonOptionsMenu`.
  const bool for_editing_list_;
  const Direction direction_index_;

  // Layer for edit label pulse animation.
  std::unique_ptr<ui::Layer> pulse_layer_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_
