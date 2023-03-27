// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_H_

#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// Observes changes in the windows of the SnapGroup and manages the windows
// accordingly.
class SnapGroup : public aura::WindowObserver {
 public:
  SnapGroup(aura::Window* window1, aura::Window* window2);
  SnapGroup(const SnapGroup&) = delete;
  SnapGroup& operator=(const SnapGroup&) = delete;
  ~SnapGroup() override;

  // aura::WindowObserver:
  // TODO: Implement `OnWindowParentChanged`.
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowDestroying(aura::Window* window) override;

  aura::Window* window1() const { return window1_; }
  aura::Window* window2() const { return window2_; }

 private:
  friend class SnapGroupController;

  // Observes the windows that are added in the `SnapGroup`.
  void StartObservingWindows(aura::Window* window1, aura::Window* window2);

  // Stops observing the windows when the `SnapGroup` gets destructed.
  void StopObservingWindows();

  aura::Window* window1_;
  aura::Window* window2_;
};

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_H_