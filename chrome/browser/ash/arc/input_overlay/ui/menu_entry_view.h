// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_

#include "ash/constants/ash_features.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

// MenuEntryView is for GIO menu entry button.
class MenuEntryView : public views::ImageButton {
 public:
  explicit MenuEntryView(PressedCallback callback);

  MenuEntryView(const MenuEntryView&) = delete;
  MenuEntryView& operator=(const MenuEntryView&) = delete;

  ~MenuEntryView() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Used for testing.
  void set_beta(bool beta) { beta_ = beta; }

 private:
  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd();

  // The position when starting to drag.
  gfx::Point start_drag_pos_;

  // TODO(b/253646354): This can be removed when removing the flag.
  bool beta_ = ash::features::IsArcInputOverlayBetaEnabled();

  // If this view is in a dragging state.
  bool is_dragging_ = false;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
