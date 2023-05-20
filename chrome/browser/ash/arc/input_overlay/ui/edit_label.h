// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace arc::input_overlay {

class Action;

// EditLabel shows input mappings and can be edited to change mappings.
class EditLabel : public views::LabelButton {
 public:
  METADATA_HEADER(EditLabel);
  explicit EditLabel(Action* action, size_t index = 0);

  EditLabel(const EditLabel&) = delete;
  EditLabel& operator=(const EditLabel&) = delete;
  ~EditLabel() override;

 private:
  void Init();
  std::u16string CalculateAccessibleName();
  bool IsInputUnbound();

  void SetToDefault();
  void SetToFocused();
  void SetToUnbound();

  // views::View:
  void OnFocus() override;
  void OnBlur() override;

  raw_ptr<Action> action_;
  size_t index_ = 0;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_EDIT_LABEL_H_
