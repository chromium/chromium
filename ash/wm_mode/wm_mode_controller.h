// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_WM_MODE_CONTROLLER_H_
#define ASH_WM_MODE_WM_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/layer_owner.h"
#include "ui/events/event_handler.h"

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
                                    public ui::LayerDelegate {
 public:
  WmModeController();
  WmModeController(const WmModeController&) = delete;
  WmModeController& operator=(const WmModeController&) = delete;
  ~WmModeController() override;

  static WmModeController* Get();

  bool is_active() const { return is_active_; }
  aura::Window* selected_window() { return selected_window_; }

  // Toggles the active state of this mode.
  void Toggle();

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  base::StringPiece GetLogContext() const override;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}

  // Returns true if the given `root` window is being dimmed.
  bool IsRootWindowDimmedForTesting(aura::Window* root) const;

 private:
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

  bool is_active_ = false;

  // The current root window the layer of `this` belongs to. It's always nullptr
  // when WM Mode is inactive.
  raw_ptr<aura::Window, ExperimentalAsh> current_root_ = nullptr;

  // The window that got selected as the top-most one at the most recent
  // received located event. This window (if available) will be the one that
  // receives all the gestures supported by this mode.
  raw_ptr<aura::Window, ExperimentalAsh> selected_window_ = nullptr;

  // When WM Mode is enabled, we dim all the displays as an indication of this
  // special mode being active, which disallows the normal interaction with
  // windows and their contents, and enables the various gestures supported by
  // this mode.
  // `dimmers_` maps each root window to its associated dimmer.
  base::flat_map<aura::Window*, std::unique_ptr<WindowDimmer>> dimmers_;
};

}  // namespace ash

#endif  // ASH_WM_MODE_WM_MODE_CONTROLLER_H_
