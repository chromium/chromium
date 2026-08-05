// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_
#define ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

// Pure virtual base class for window occlusion calculators used by the desk
// bar.
class ASH_EXPORT WindowOcclusionCalculator {
 public:
  virtual ~WindowOcclusionCalculator() = default;

  // Returns the cached occlusion state of the given `window`.
  virtual aura::Window::OcclusionState GetOcclusionState(
      aura::Window* window) const = 0;

  // Internally records a snapshot of the occlusion state for all
  // `parent_windows_to_snapshot` and their descendants.
  virtual void SnapshotOcclusionStateForWindows(
      const aura::Window::Windows& parent_windows_to_snapshot) = 0;

  // Temporarily pauses all calculations.
  virtual std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
  Pause() = 0;

  virtual base::WeakPtr<WindowOcclusionCalculator> AsWeakPtr() = 0;

  // Factory method to spawn the active WindowOcclusionCalculator
  // implementation.
  static std::unique_ptr<WindowOcclusionCalculator> Create();
};

}  // namespace ash

#endif  // ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_
