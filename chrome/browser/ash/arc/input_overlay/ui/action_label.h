// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_

#include <string>

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/views/controls/button/label_button.h"

namespace arc::input_overlay {

constexpr char kUnknownBind[] = "?";

// TODO(cuicuiruan): Currently, it shows the dom_code.
// Will replace it with showing the result of dom_key / keyboard key depending
// on different keyboard layout.
std::string GetDisplayText(const ui::DomCode code);

// ActionLabel shows text mapping hint for each action.
class ActionLabel : public views::LabelButton {
 public:
  ActionLabel();
  ActionLabel(const ActionLabel&) = delete;
  ActionLabel& operator=(const ActionLabel&) = delete;
  ~ActionLabel() override;

  static std::unique_ptr<ActionLabel> CreateTextActionLabel(
      const std::string& text);
  static std::unique_ptr<ActionLabel> CreateImageActionLabel(
      MouseAction mouse_action);

  void SetTextActionLabel(const std::string& text);
  void SetImageActionLabel(MouseAction mouse_action);
  void SetDisplayMode(DisplayMode mode);
  // Return true if it has focus before clear focus.
  bool ClearFocus();

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

  void set_mouse_action(MouseAction mouse_action) {
    mouse_action_ = mouse_action;
  }

 private:
  void SetToViewMode();
  void SetToEditMode();
  // In edit mode without mouse hover or focus.
  void SetToEditDefault();
  // In edit mode when mouse hovers.
  void SetToEditHover();
  // In edit mode when this view is focused.
  void SetToEditFocus();
  // In edit mode when there is edit error.
  void SetToEditError();
  // In edit mode when the input is unbound.
  void SetToEditUnbindInput();

  bool IsInputUnbound();

  MouseAction mouse_action_ = MouseAction::NONE;
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
