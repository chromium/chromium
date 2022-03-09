// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace arc {
namespace input_overlay {
class TouchInjector;
class InputMappingView;
class InputMenuView;
class ActionEditMenu;

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

  void AddActionEditMenu(ActionView* anchor);
  void RemoveActionEditMenu();

 private:
  friend class DisplayOverlayControllerTest;
  friend class InputMenuView;
  friend class InputMappingView;

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
  bool HasMenuView() const;
  void SetInputMappingVisible(bool visible);
  bool GetInputMappingViewVisible() const;

  void SetTouchInjectorEnable(bool enable);
  bool GetTouchInjectorEnable();

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();

  TouchInjector* touch_injector() { return touch_injector_; }

  TouchInjector* touch_injector_;

  // References to UI elements owned by the overlay widget.
  InputMappingView* input_mapping_view_ = nullptr;
  DisplayMode display_mode_ = DisplayMode::kNone;
  InputMenuView* input_menu_view_ = nullptr;
  views::ImageButton* menu_entry_ = nullptr;
  ActionEditMenu* action_edit_menu_ = nullptr;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
