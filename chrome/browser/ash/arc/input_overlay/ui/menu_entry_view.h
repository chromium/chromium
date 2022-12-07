// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

// MenuEntryView is for GIO menu entry button.
class MenuEntryView : public views::ImageButton {
 public:
  using OnDragEndCallback =
      base::RepeatingCallback<void(absl::optional<gfx::Point>)>;

  MenuEntryView(PressedCallback pressed_callback,
                OnDragEndCallback on_position_changed_callback);
  MenuEntryView(const MenuEntryView&) = delete;
  MenuEntryView& operator=(const MenuEntryView&) = delete;
  ~MenuEntryView() override;

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Used for testing.
  void set_allow_reposition(bool allow) { allow_reposition_ = allow; }

 private:
  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd();

  OnDragEndCallback on_drag_end_callback_;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
  // This view's position when drag starts.
  gfx::Point start_drag_view_pos_;
  // If this view is in a dragging state.
  bool is_dragging_ = false;

  // TODO(b/260937747): Update or remove when removing flags
  // |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
  bool allow_reposition_ = ash::features::IsArcInputOverlayAlphaV2Enabled() ||
                           ash::features::IsArcInputOverlayBetaEnabled();
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
