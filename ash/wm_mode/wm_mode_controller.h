// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_WM_MODE_CONTROLLER_H_
#define ASH_WM_MODE_WM_MODE_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm_mode/pie_menu_view.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class LocatedEvent;
}  // namespace ui

namespace ash {

class WindowDimmer;

// Controls an *experimental* feature that allows users to easily layout, resize
// and position their windows using only mouse and touch gestures without having
// to be very precise at dragging, or targeting certain buttons. A demo of an
// exploration prototype can be watched at https://crbug.com/1348416.
// Please note this feature may never be released.
class ASH_EXPORT WmModeController : public ShellObserver,
                                    public ui::EventHandler,
                                    public ui::LayerOwner,
                                    public ui::LayerDelegate,
                                    public aura::WindowObserver,
                                    public PieMenuView::Delegate,
                                    public DesksController::Observer {
 public:
  enum PieMenuButtonIds {
    kSnapButtonId = 0,
    kMoveToDeskButtonId = 1,
    kResizeButtonId = 2,

    kDeskButtonIdStart = 3,
    // Keep this range reserved for desk button IDs.
    kDeskButtonIdEnd = kDeskButtonIdStart + desks_util::kDesksUpperLimit - 1,
  };

  WmModeController();
  WmModeController(const WmModeController&) = delete;
  WmModeController& operator=(const WmModeController&) = delete;
  ~WmModeController() override;

  static WmModeController* Get();

  bool is_active() const { return is_active_; }
  aura::Window* selected_window() { return selected_window_; }
  views::Widget* pie_menu_widget() { return pie_menu_widget_.get(); }

  // Toggles the active state of this mode.
  void Toggle();

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  std::string_view GetLogContext() const override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // PieMenuView::Delegate:
  void OnPieMenuButtonPressed(int button_id) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

 private:
  friend class WmModeTests;

  void UpdateDimmers();

  // Updates the state of all the WM Mode tray buttons on all displays.
  void UpdateTrayButtons();

  // Handles both mouse and touch events.
  void OnLocatedEvent(ui::LocatedEvent* event);

  // Creates the layer owned by `this`, but doesn't attach it to the layer
  // hierarchy. This can be done by calling `MaybeChangeRoot()`. This function
  // can only be called when WM Mode is active.
  void CreateLayer();

  // Adds the layer owned by `this` to the layer hierarchy of the given
  // `new_root` if it's different than `current_root_`. This function can only
  // be called when WM Mode is active `layer()` is valid.
  void MaybeChangeRoot(aura::Window* new_root);

  // Sets the given `window` as the currently selected window. If `window` is
  // nullptr, the selected window is cleared.
  void SetSelectedWindow(aura::Window* window);

  // Schedules repainting the contents of the layer owned by `this`.
  void ScheduleRepaint();

  // Builds the pie menu widget.
  void BuildPieMenu();

  // If the pie menu is available, it rebuilds the move-to-desk sub menu items.
  void MaybeRebuildMoveToDeskSubMenu();

  // Returns true if the given `event_target` is contained within the window
  // tree of the pie menu if it exists.
  bool IsTargetingPieMenu(aura::Window* event_target) const;

  // Gets the top-most window that can be selected for WM Mode operations at the
  // given `screen_location`.
  aura::Window* GetTopMostWindowAtPoint(
      const gfx::Point& screen_location) const;

  // Refreshes the visibility and the bounds of the pie menu (if it exists).
  void MaybeRefreshPieMenu();

  // Moves the `selected_window_` to the desk at the given `index`.
  void MoveSelectedWindowToDeskAtIndex(int index);

  bool is_active_ = false;

  // The current root window the layer of `this` belongs to. It's always nullptr
  // when WM Mode is inactive.
  raw_ptr<aura::Window> current_root_ = nullptr;

  // The window that got selected as the top-most one at the most recent
  // received located event. This window (if available) will be the one that
  // receives all the gestures supported by this mode.
  raw_ptr<aura::Window, DanglingUntriaged> selected_window_ = nullptr;

  views::UniqueWidgetPtr pie_menu_widget_;
  raw_ptr<PieMenuView> pie_menu_view_ = nullptr;

  // When WM Mode is enabled, we dim all the displays as an indication of this
  // special mode being active, which disallows the normal interaction with
  // windows and their contents, and enables the various gestures supported by
  // this mode.
  // `dimmers_` maps each root window to its associated dimmer.
  base::flat_map<aura::Window*, std::unique_ptr<WindowDimmer>> dimmers_;

  // The screen location of the last received release located event.
  // Valid only if we receive a release located event, and only until
  // `OnLocatedEvent()` returns.
  std::optional<gfx::Point> last_release_event_screen_point_;
};

}  // namespace ash

#endif  // ASH_WM_MODE_WM_MODE_CONTROLLER_H_
