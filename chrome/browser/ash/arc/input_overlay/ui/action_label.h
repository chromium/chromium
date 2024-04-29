// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/label_button.h"

namespace arc::input_overlay {

class ActionView;

// ActionLabel shows text mapping hint for each action.
class ActionLabel : public views::LabelButton {
  METADATA_HEADER(ActionLabel, views::LabelButton)
 public:
  static std::vector<raw_ptr<ActionLabel, VectorExperimental>> Show(
      views::View* parent,
      ActionType action_type,
      const InputElement& input_element,
      TapLabelPosition label_position = TapLabelPosition::kTopLeft);

  explicit ActionLabel(MouseAction mouse_action);
  explicit ActionLabel(const std::u16string& text, size_t index = 0);

  ActionLabel(const ActionLabel&) = delete;
  ActionLabel& operator=(const ActionLabel&) = delete;
  ~ActionLabel() override;

  void Init();

  void SetTextActionLabel(const std::u16string& text);
  void SetImageActionLabel(MouseAction mouse_action);
  void SetDisplayMode(DisplayMode mode);
  void RemoveNewState();
  void ClearFocus();
  // It is possible that multiple labels are in one ActionView and these labels
  // are called sibling labels. This label reacts to sibling's focus change.
  void OnSiblingUpdateFocus(bool sibling_focused);

  ActionView* GetParent();

  virtual void UpdateBounds() = 0;
  virtual void UpdateLabelPositionType(TapLabelPosition label_position) = 0;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void ChildPreferredSizeChanged(View* child) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

  void set_mouse_action(MouseAction mouse_action) {
    mouse_action_ = mouse_action;
  }

  void set_touch_point_size(gfx::Size size) { touch_point_size_ = size; }

 protected:
  int radius_ = 0;
  size_t index_ = 0;

  MouseAction mouse_action_ = MouseAction::NONE;

  DisplayMode display_mode_ = DisplayMode::kView;
  // This view needs to set position relative to the size of TouchPoint.
  // ActionTap and ActionView have different TouchPoint size.
  gfx::Size touch_point_size_;

 private:
  void OnButtonPressed();

  void SetToViewMode();
  void SetToEditMode();
  // In edit mode without mouse hover or focus.
  void SetToEditDefault();
  // In edit mode when mouse hovers or not.
  void SetToEditHover(bool hovered);
  // In edit mode when this view is focused.
  void SetToEditFocus();
  // In edit mode when there is edit error.
  void SetToEditError();
  // In edit mode when the input is unbound.
  void SetToEditUnbindInput();
  // In edit mode of ActionMoveView with four keys, when one label is focused,
  // the other labels turn to edit inactive visually.
  void SetToEditInactive();

  void SetBackgroundForEdit();

  bool IsInputUnbound();
  // Calculate the accessible name.
  std::u16string CalculateAccessibleName();
};
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_ACTION_LABEL_H_
