// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_

#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

// Represent elements in the TouchPoint. It can be touch point center, inside
// stroke or outside stroke.
class TouchPointElement : public views::View {
 public:
  TouchPointElement();
  ~TouchPointElement() override;

  virtual void SetToDefault() = 0;
  virtual void SetToHover() = 0;
  virtual void SetToDrag() = 0;
};

// TouchPoint indicates the touch point for each action and shows up in the edit
// mode.
class TouchPoint : public views::View {
 public:
  static TouchPoint* Show(views::View* parent,
                          ActionType action_type,
                          const gfx::Point& center_pos);
  static gfx::Size GetSize(ActionType action_type);

  explicit TouchPoint(const gfx::Point& center_pos);
  TouchPoint(const TouchPoint&) = delete;
  TouchPoint& operator=(const TouchPoint&) = delete;
  ~TouchPoint() override;

  virtual void Init();

  void OnCenterPositionChanged(const gfx::Point& point);

  void SetToDefault();
  void SetToHover();
  void SetToDrag();

  void ApplyMouseEntered(const ui::MouseEvent& event);
  void ApplyMouseExited(const ui::MouseEvent& event);
  bool ApplyMousePressed(const ui::MouseEvent& event);
  bool ApplyMouseDragged(const ui::MouseEvent& event);
  void ApplyMouseReleased(const ui::MouseEvent& event);
  void ApplyGestureEvent(ui::GestureEvent* event);

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;

 protected:
  raw_ptr<TouchPointElement> touch_center_;
  raw_ptr<TouchPointElement> touch_inside_stroke_;
  raw_ptr<TouchPointElement> touch_outside_stroke_;

 private:
  gfx::Point center_pos_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TOUCH_POINT_H_
