// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TARGET_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TARGET_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/db/proto/app_data.pb.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

class DisplayOverlayController;

// View shows target circle, crossed line and touch point in button placement
// mode.
class TargetView : public views::View {
  METADATA_HEADER(TargetView, views::View)

 public:
  TargetView(DisplayOverlayController* controller, ActionType action_type);

  TargetView(const TargetView&) = delete;
  TargetView& operator=(const TargetView&) = delete;

  ~TargetView() override;

  void UpdateWidgetBounds();

  // Returns the bounds of the overall circle in this view.
  gfx::Rect GetTargetCircleBounds() const;

 private:
  friend class TargetViewTest;

  // The overall target circle radius.
  int GetCircleRadius() const;
  // The target circle ring radius excluding the ring thickness.
  int GetCircleRingRadius() const;
  // The padding that `center_` can't access.
  int GetPadding() const;

  // Clamps `center_` so that the target circle can show completely and
  // constraint the action position.
  void ClampCenter();

  // Called when `center_` is updated.
  void OnCenterChanged();

  // Sets cursor intial position when this view shows up.
  void MoveCursorToViewCenter();

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseMoved(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnPaintBackground(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  const raw_ptr<DisplayOverlayController> controller_;
  const ActionType action_type_;

  // The local center position of the target circle.
  gfx::Point center_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_TARGET_VIEW_H_
