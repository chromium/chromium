// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_H_

#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Observes changes in the windows of the SnapGroup and manages the windows
// accordingly.
class SnapGroup : public aura::WindowObserver, public WindowStateObserver {
 public:
  SnapGroup(aura::Window* window1, aura::Window* window2);
  SnapGroup(const SnapGroup&) = delete;
  SnapGroup& operator=(const SnapGroup&) = delete;
  ~SnapGroup() override;

  aura::Window* window1() const { return window1_; }
  aura::Window* window2() const { return window2_; }

  // Returns the topmost window in the snap group.
  aura::Window* GetTopMostWindowInGroup() const;

  // Minimizes the windows in the snap group.
  void MinimizeWindows();

  // Swaps the windows in the snap group.
  void SwapWindows();

  // aura::WindowObserver:
  // TODO: Implement `OnWindowParentChanged`.
  void OnWindowDestroying(aura::Window* window) override;

  // WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;

 private:
  friend class SnapGroupController;

  // Observes the windows that are added in the `SnapGroup`.
  void StartObservingWindows();

  // Stops observing the windows when the `SnapGroup` gets destructed.
  void StopObservingWindows();

  // Shrinks the bounds of both windows in snap group `on_snap_group_added` or
  // expands the bounds of both windows in snap group when `on_snap_group_added`
  // is false, i.e. on snap group removed.
  void RefreshWindowBoundsInSnapGroup(bool on_snap_group_added);

  // True while we are updating the windows during a swap.
  bool is_swapping_ = false;

  raw_ptr<aura::Window> window1_;
  raw_ptr<aura::Window> window2_;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_H_
