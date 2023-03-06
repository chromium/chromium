// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

class DisplayOverlayController;

// MenuEntryView is for GIO menu entry button.
class MenuEntryView : public views::ImageButton {
 public:
  using OnPositionChangedCallback =
      base::RepeatingCallback<void(bool, absl::optional<gfx::Point>)>;

  MenuEntryView(PressedCallback pressed_callback,
                OnPositionChangedCallback on_position_changed_callback,
                DisplayOverlayController* display_overlay_controller);
  MenuEntryView(const MenuEntryView&) = delete;
  MenuEntryView& operator=(const MenuEntryView&) = delete;
  ~MenuEntryView() override;

  // Change hover state for menu entry button.
  void ChangeHoverState(bool is_hovered);

  // views::View:
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;

  // Used for testing.
  void set_allow_reposition(bool allow) { allow_reposition_ = allow; }

 private:
  // Drag operations.
  void OnDragStart(const ui::LocatedEvent& event);
  void OnDragUpdate(const ui::LocatedEvent& event);
  void OnDragEnd();

  // TODO(b/260937747) For Alpha version, this view is not movable. Cancel
  // located event and reset event target when the located event doesn't
  // released on top of this view. This can be removed when removing the AlphaV2
  // flag.
  void MayCancelLocatedEvent(const ui::LocatedEvent& event);

  OnPositionChangedCallback on_position_changed_callback_;

  // Change menu entry properties if currently in dragging state.
  void ChangeMenuEntryOnDrag(bool is_dragging);
  // Set cusor type.
  void SetCursor(ui::mojom::CursorType cursor_type);

  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;

  // LocatedEvent's position when drag starts.
  gfx::Point start_drag_event_pos_;
  // This view's position when drag starts.
  gfx::Point start_drag_view_pos_;
  // If this view is in a dragging state.
  bool is_dragging_ = false;

  // The current hover state for the menu entry.
  bool hover_state_ = false;

  // TODO(b/260937747): Update or remove when removing flags
  // |kArcInputOverlayAlphaV2| or |kArcInputOverlayBeta|.
  bool allow_reposition_ = ash::features::IsArcInputOverlayAlphaV2Enabled() ||
                           ash::features::IsArcInputOverlayBetaEnabled();
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
