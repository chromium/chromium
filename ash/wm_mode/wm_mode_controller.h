// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_MODE_WM_MODE_CONTROLLER_H_
#define ASH_WM_MODE_WM_MODE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/shell_observer.h"
#include "base/containers/flat_map.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class WindowDimmer;

// Controls an *experimental* feature that allows users to easily layout, resize
// and position their windows using only mouse and touch gestures without having
// to be very precise at dragging, or targeting certain buttons. A demo of an
// exploration prototype can be watched at https://crbug.com/1348416.
// Please note this feature may never be released.
class ASH_EXPORT WmModeController : public ShellObserver {
 public:
  WmModeController();
  WmModeController(const WmModeController&) = delete;
  WmModeController& operator=(const WmModeController&) = delete;
  ~WmModeController() override;

  static WmModeController* Get();

  bool is_active() const { return is_active_; }

  // Toggles the active state of this mode.
  void Toggle();

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;

  // Returns true if the given `root` window is being dimmed.
  bool IsRootWindowDimmedForTesting(aura::Window* root) const;

 private:
  void UpdateDimmers();

  // Updates the state of all the WM Mode tray buttons on all displays.
  void UpdateTrayButtons();

  bool is_active_ = false;

  // When WM Mode is enabled, we dim all the displays as an indication of this
  // special mode being active, which disallows the normal interaction with
  // windows and their contents, and enables the various gestures supported by
  // this mode.
  // `dimmers_` maps each root window to its associated dimmer.
  base::flat_map<aura::Window*, std::unique_ptr<WindowDimmer>> dimmers_;
};

}  // namespace ash

#endif  // ASH_WM_MODE_WM_MODE_CONTROLLER_H_
