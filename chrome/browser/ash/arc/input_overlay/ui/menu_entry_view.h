// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_

#include "ash/constants/ash_features.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/ui/reposition_controller.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/image_button.h"

namespace arc::input_overlay {

class DisplayOverlayController;

// MenuEntryView is for GIO menu entry button.
class MenuEntryView : public views::ImageButton {
  METADATA_HEADER(MenuEntryView, views::ImageButton)

 public:
  using OnPositionChangedCallback =
      base::RepeatingCallback<void(bool, std::optional<gfx::Point>)>;

  static MenuEntryView* Show(
      PressedCallback pressed_callback,
      OnPositionChangedCallback on_position_changed_callback,
      DisplayOverlayController* display_overlay_controller);

  MenuEntryView(PressedCallback pressed_callback,
                OnPositionChangedCallback on_position_changed_callback,
                DisplayOverlayController* display_overlay_controller);
  MenuEntryView(const MenuEntryView&) = delete;
  MenuEntryView& operator=(const MenuEntryView&) = delete;
  ~MenuEntryView() override;

  // Change hover state for menu entry button.
  void ChangeHoverState(bool is_hovered);

  // Callbacks related to reposition operations.
  void OnFirstDraggingCallback();
  void OnMouseDragEndCallback();
  void OnGestureDragEndCallback();
  void OnKeyReleasedCallback();

  // views::View:
  void AddedToWidget() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;

 private:
  void Init();
  gfx::Point CalculatePosition() const;

  OnPositionChangedCallback on_position_changed_callback_;

  // Change menu entry properties if currently in dragging state.
  void ChangeMenuEntryOnDrag(bool is_dragging);
  // Set cusor type.
  void SetCursor(ui::mojom::CursorType cursor_type);

  void SetRepositionController();

  const raw_ptr<DisplayOverlayController> display_overlay_controller_ = nullptr;

  std::unique_ptr<RepositionController> reposition_controller_;

  // The current hover state for the menu entry.
  bool hover_state_ = false;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_UI_MENU_ENTRY_VIEW_H_
