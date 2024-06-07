// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_
#define ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_multi_source_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

// Calculates occlusion state for a set of `aura::Window`s and notifies
// observers whenever the occlusion changes.
//
// Context:
// When rendering the desk bar, we only want to mirror the windows in each desk
// that are actually visible for performance reasons. Thus, each desk's window's
// occlusion state is required.
//
// There are 2 issues with using the global `aura::WindowOcclusionTracker`
// inside `aura::Env` (i.e. `aura::Window::GetOcclusionState()` to get this
// information):
// 1) Window occlusion tracking is paused when opening overview mode for a
//    separate performance reason. This prevents this particular optimization
//    from getting each window's occlusion state.
// 2) If we turn on a window's occlusion tracking for this optimization, it's on
//    permanently for the rest of the ChromeOS session (resulting in possibly
//    unnecessary calculations for an indefinite amount of time). We really only
//    need the occlusion state for the few seconds that the desk bar is open.
//
// To solve this, a separate `aura::WindowOcclusionTracker` instance is created
// within `WindowOcclusionCalculator` to compute windows' occlusion state and is
// only alive for the duration of the desk bar. The key here is that this
// transient tracker uses a different `aura::WindowOcclusionChangeBuilder`
// implementation that does not call `aura::Window::SetOcclusionInfo()`. Rather,
// it passively observes the occlusion state calculated for each window and
// makes this information available to the caller. The global
// `aura::WindowOcclusionTracker` inside `aura::Env` is still the only one in
// the system mutating the `aura::Window` occlusion state.
class ASH_EXPORT WindowOcclusionCalculator : public aura::WindowObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Invoked whenever the occlusion state of a tracked window changes. The
    // caller may synchronously call `GetOcclusionState()` to retrieve the
    // `window`'s new state.
    virtual void OnWindowOcclusionChanged(aura::Window* window) = 0;
  };

  WindowOcclusionCalculator();
  WindowOcclusionCalculator(const WindowOcclusionCalculator&) = delete;
  WindowOcclusionCalculator& operator=(const WindowOcclusionCalculator&) =
      delete;
  ~WindowOcclusionCalculator() override;

  // Returns the current occlusion state of the given `window`. The occlusion
  // state is only `UNKNOWN` in one of these cases:
  // * The `window` is not currently being tracked via `AddObserver()`.
  // * The `window` was being tracked and has since been destroyed.
  // * The `window` is being tracked but was just added to the window tree and
  //   its occlusion cannot be calculated yet. It should be available
  //   imminently, at which time an `OnWindowOcclusionChanged()` notification
  //   will fire.
  aura::Window::OcclusionState GetOcclusionState(aura::Window* window) const;

  // Starts tracking the occlusion state of all windows in
  // `parent_windows_to_track` and their descendants. The `observer' is notified
  // afterwards if the occlusion state of any of the aforementioned windows
  // changes. Each window in `parent_windows_to_track` is forced to be visible
  // before calculating the occlusion of them and their descendants.
  //
  // Multiple observers may be registered for the same parent window. It is also
  // OK if one of `parent_windows_to_track` is a descendant of a parent window
  // that is already being tracked. No restrictions here.
  void AddObserver(const aura::Window::Windows& parent_windows_to_track,
                   Observer* observer);

  // Removes `observer`; this is a no-op if `observer` has not been added.
  void RemoveObserver(Observer* observer);

 private:
  class ObservationState;
  class WindowOcclusionChangeBuilderImpl;

  using WindowOcclusionMap =
      base::flat_map<raw_ptr<aura::Window>, aura::Window::OcclusionState>;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  void SetOcclusionState(aura::Window* window,
                         aura::Window::OcclusionState occlusion_state);
  void TrackOcclusionChangesForAllDescendants(aura::Window* window);

  // Holds the current occlusion state for all tracked windows. This includes
  // parent windows being observed and their descendants.
  //
  // Should outlive `occlusion_tracker_` since `occlusion_tracker_` writes to
  // this map whenever a window's occlusion state changes.
  WindowOcclusionMap occlusion_map_;

  aura::WindowOcclusionTracker occlusion_tracker_;

  // Map from parent window to the observers that should be notified when the
  // parent window's occlusion changes or any of its descendants' occlusion
  // changes.
  //
  // Should be destroyed before the `occlusion_tracker_` since the
  // `ObservationState` holds a raw pointer to the `occlusion_tracker_` via its
  // `forced_visibility_` member.
  base::flat_map<raw_ptr<aura::Window>, std::unique_ptr<ObservationState>>
      occlusion_change_observers_;

  // Only for monitoring when a tracked `aura::Window` gets destroyed. See
  // `OnWindowDestroyed()` implementation.
  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      tracked_window_observations_{this};

  base::WeakPtrFactory<WindowOcclusionCalculator> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_DESKS_WINDOW_OCCLUSION_CALCULATOR_H_
