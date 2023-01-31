// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/views/controls/button/label_button.h"

namespace arc::input_overlay {

// TODO(cuicuiruan): Currently, it shows the dom_code.
// Will replace it with showing the result of dom_key / keyboard key depending
// on different keyboard layout.
std::string GetDisplayText(const ui::DomCode code);

// ActionLabel shows text mapping hint for each action.
class ActionLabel : public views::LabelButton {
 public:
  static std::vector<ActionLabel*> Show(
      views::View* parent,
      ActionType action_type,
      const InputElement& input_element,
      int radius,
      TapLabelPosition label_position = TapLabelPosition::kTopLeft);

  ActionLabel(int radius, MouseAction mouse_action);
  ActionLabel(int radius, const std::string& text);
  ActionLabel(int radius, const std::string& text, int index);

  ActionLabel(const ActionLabel&) = delete;
  ActionLabel& operator=(const ActionLabel&) = delete;
  ~ActionLabel() override;

  void Init();

  void SetTextActionLabel(const std::string& text);
  void SetImageActionLabel(MouseAction mouse_action);
  void SetDisplayMode(DisplayMode mode);
  // Return true if it has focus before clear focus.
  bool ClearFocus();

  // TODO(b/260937747): Update or remove when removing flags
  // |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
  // The label layout design is updated. This is used to update bounds
  // for Alpha version.
  virtual void UpdateBoundsAlpha() = 0;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void ChildPreferredSizeChanged(View* child) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

  void set_mouse_action(MouseAction mouse_action) {
    mouse_action_ = mouse_action;
  }

 protected:
  int radius_ = 0;
  size_t index_ = 0;

  MouseAction mouse_action_ = MouseAction::NONE;

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
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
