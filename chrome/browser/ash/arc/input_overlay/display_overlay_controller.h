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
class TouchInjector;
class InputMenuView;

// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog.
class DisplayOverlayController {
 public:
  explicit DisplayOverlayController(TouchInjector* touch_injector);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController();

  void OnWindowBoundsChanged();
  void SetDisplayMode(DisplayMode mode);
  // Get the bounds of |overlay_menu_entry_| in contents view.
  absl::optional<gfx::Rect> GetOverlayMenuEntryBounds();

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();

 private:
  friend class DisplayOverlayControllerTest;
  friend class InputMenuView;

  // InputMappingView is the whole view of the input mappings.
  class InputMappingView;

  void AddOverlay();
  void RemoveOverlayIfAny();

  void AddInputMappingView(views::Widget* overlay_widget);
  void AddMenuEntryView(views::Widget* overlay_widget);
  void OnMenuEntryPressed();

  void RemoveInputMenuView();
  void RemoveInputMappingView();
  void RemoveMenuEntryView();

  views::Widget* GetOverlayWidget();
  gfx::Point CalculateMenuEntryPosition();
  gfx::Rect get_menu_entry_bounds() const { return menu_entry_->bounds(); }
  bool HasMenuView() const;
  void SetInputMappingVisible(bool visible);
  bool GetInputMappingViewVisible() const;

  TouchInjector* touch_injector_;

  // References to UI elements owned by the overlay widget.
  InputMappingView* input_mapping_view_ = nullptr;
  DisplayMode display_mode_ = DisplayMode::kNone;
  InputMenuView* input_menu_view_ = nullptr;
  views::ImageButton* menu_entry_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
