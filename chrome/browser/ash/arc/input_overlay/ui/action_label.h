// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_

#include <string>

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/views/controls/label.h"

namespace arc {
namespace input_overlay {
// TODO(cuicuiruan): Currently, it shows the dom_code.
// Will replace it with showing the result of dom_key / keyboard key depending
// on different keyboard layout.
std::string GetDisplayText(const ui::DomCode code);

// ActionLabel shows text mapping hint for each action.
class ActionLabel : public views::Label {
 public:
  ActionLabel();
  explicit ActionLabel(const std::u16string& text);

  ActionLabel(const ActionLabel&) = delete;
  ActionLabel& operator=(const ActionLabel&) = delete;
  ~ActionLabel() override;

  void SetToViewMode();
  void SetToEditMode();
  void SetToEditedUnBind();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnFocus() override;
  void OnBlur() override;

 private:
  void SetToEditFocus();
};
}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
