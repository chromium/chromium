// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_NEW_WINDOW_OCCLUSION_CALCULATOR_H_
#define ASH_WM_DESKS_NEW_WINDOW_OCCLUSION_CALCULATOR_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/window_occlusion_calculator.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

// Concrete implementation of `WindowOcclusionCalculator` that uses the global
// `aura::WindowOcclusionTracker` to compute window occlusion via synchronous
// snapshots.
class ASH_EXPORT NewWindowOcclusionCalculator
    : public WindowOcclusionCalculator,
      public DesksController::Observer,
      public Desk::Observer {
 public:
  NewWindowOcclusionCalculator();
  NewWindowOcclusionCalculator(const NewWindowOcclusionCalculator&) = delete;
  NewWindowOcclusionCalculator& operator=(const NewWindowOcclusionCalculator&) =
      delete;
  ~NewWindowOcclusionCalculator() override;

  // WindowOcclusionCalculator:
  aura::Window::OcclusionState GetOcclusionState(
      aura::Window* window) const override;
  void SnapshotOcclusionStateForWindows(
      const aura::Window::Windows& containers_to_snapshot) override;
  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause> Pause() override;
  base::WeakPtr<WindowOcclusionCalculator> AsWeakPtr() override;

  using DesksController::Observer::OnDeskNameChanged;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {}

  // Desk::Observer:
  void OnContentChanged() override;
  void OnDeskNameChanged(const std::u16string& new_name) override {}
  void OnDeskDestroyed(const Desk* desk) override;

 private:
  void Reset();

  // Caches computed occlusion states from the last snapshot, grouped by
  // container window.
  base::flat_map<aura::Window*,
                 base::flat_map<aura::Window*, aura::Window::OcclusionState>>
      cached_states_;

  base::ScopedObservation<DesksController, DesksController::Observer>
      desks_controller_observation_{this};
  base::ScopedMultiSourceObservation<Desk, Desk::Observer> desk_observations_{
      this};

  base::WeakPtrFactory<NewWindowOcclusionCalculator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_NEW_WINDOW_OCCLUSION_CALCULATOR_H_
