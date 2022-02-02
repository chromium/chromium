// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace arc {
namespace input_overlay {
// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog.
class DisplayOverlayController {
 public:
  explicit DisplayOverlayController(TouchInjector* touch_injector);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController();

  void OnWindowBoundsChanged();

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();

 private:
  friend class DisplayOverlayControllerTest;

  // InputMappingView is the whole view of the input mappings.
  class InputMappingView;

  void AddOverlay();
  void RemoveOverlayIfAny();
  void AddOverlayChildrenViews();
  void AddInputMappingView(views::Widget* overlay_widget);
  void AddInputOverlayMenuView(views::Widget* overlay_widget);
  void OnMenuAnchorPressed();
  void RemoveInputMappingView();
  views::Widget* GetOverlayWidget();
  gfx::Point CalculateMenuAnchorPosition();
  gfx::Rect get_overlay_menu_anchor_bounds() const {
    return overlay_menu_anchor_->bounds();
  }

  TouchInjector* touch_injector_;

  // References to UI elements owned by the overlay widget.
  InputMappingView* input_mapping_view_ = nullptr;
  views::ImageButton* overlay_menu_anchor_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
