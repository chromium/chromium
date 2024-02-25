// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_

#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}  // namespace gfx

namespace ui {
class Cursor;
}  // namespace ui

namespace arc::input_overlay {

// TouchPoint indicates the touch point for each action and shows up in the edit
// mode.
class TouchPoint : public views::View {
  METADATA_HEADER(TouchPoint, views::View)

 public:
  static TouchPoint* Show(views::View* parent,
                          ActionType action_type,
                          const gfx::Point& center_pos);

  // Gets the overall edge length of the squared view of `action_type`.
  static int GetEdgeLength(ActionType action_type);
  // Gets the overall size of the squared view of `action_type`.
  static gfx::Size GetSize(ActionType action_type);

  // Draws TouchPoint of `action_type` on `canvas`. `ui_state` is related
  // whether the TouchPoint is dragged or mouse hovered. `center` is the
  // TouchPoint's center coordinates.
  static void DrawTouchPoint(gfx::Canvas* canvas,
                             const ui::ColorProvider* color_provider,
                             ActionType action_type,
                             UIState ui_state,
                             const gfx::Point& center);

  explicit TouchPoint(const gfx::Point& center_pos);
  TouchPoint(const TouchPoint&) = delete;
  TouchPoint& operator=(const TouchPoint&) = delete;
  ~TouchPoint() override;

  virtual void Init();

  void OnCenterPositionChanged(const gfx::Point& point);

  // views::View:
  ui::Cursor GetCursor(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnFocus() override;
  void OnBlur() override;

 protected:
  void PaintBackground(gfx::Canvas* canvas, ActionType action_type);

 private:
  void SetToDefault();
  void SetToHover();
  void SetToDrag();

  gfx::Point center_pos_;
  UIState ui_state_ = UIState::kDefault;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_
