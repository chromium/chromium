// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include <algorithm>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/adapters.h"
#include "base/stl_util.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {

namespace {

// A class that observes a window that should not be destroyed inside a certain
// scope. This class is added to investigate crbug.com/937381 to see if it's
// possible that a window is destroyed while building up the mru window list.
// TODO(crbug.com/937381): Remove this class once we figure out the reason.
class ScopedWindowClosingObserver : public aura::WindowObserver {
 public:
  explicit ScopedWindowClosingObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~ScopedWindowClosingObserver() override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override { CHECK(false); }

 private:
  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ScopedWindowClosingObserver);
};

bool IsWindowConsideredActivatable(aura::Window* window) {
  DCHECK(window);
  ScopedWindowClosingObserver observer(window);
  AshFocusRules* focus_rules = Shell::Get()->focus_rules();

  // Only toplevel windows can be activated.
  if (!focus_rules->IsToplevelWindow(window))
    return false;

  if (!focus_rules->IsWindowConsideredVisibleForActivation(window))
    return false;

  if (::wm::GetActivationDelegate(window) &&
      !::wm::GetActivationDelegate(window)->ShouldActivate()) {
    return false;
  }

  return window->CanFocus();
}

// A predicate that determines whether |window| can be included in the list
// built for cycling through windows (alt + tab).
bool CanIncludeWindowInCycleList(aura::Window* window) {
  return CanIncludeWindowInMruList(window) &&
         !window_util::ShouldExcludeForCycleList(window);
}

// A predicate that determines whether |window| can be included in the list
// built for alt-tab cycling, including Android PIP windows.
bool CanIncludeWindowInCycleWithPipList(aura::Window* window) {
  return CanIncludeWindowInCycleList(window) ||
         window_util::IsArcPipWindow(window);
}

// Returns a list of windows ordered by their stacking order such that the most
// recently used window is at the front of the list.
// If |mru_windows| is passed, these windows are moved to the front of the list.
// If |desks_mru_type| is `kAllDesks`, then all active and inactive desk
// containers will be considered, otherwise only the active desk container is
// considered.
// It uses the given |can_include_window_predicate| to determine whether to
// include a window in the returned list or not.
template <class CanIncludeWindowPredicate>
MruWindowTracker::WindowList BuildWindowListInternal(
    const std::vector<aura::Window*>* mru_windows,
    DesksMruType desks_mru_type,
    CanIncludeWindowPredicate can_include_window_predicate) {
  MruWindowTracker::WindowList windows;

  const int active_desk_id = desks_util::GetActiveDeskContainerId();
  const bool active_desk_only = desks_mru_type == kActiveDesk;
  // Put the windows in the mru_windows list at the head, if it's available.
  if (mru_windows) {
    // The |mru_windows| are sorted such that the most recent window comes last,
    // hence iterate in reverse order.
    for (auto* window : base::Reversed(*mru_windows)) {
      // Exclude windows in non-switchable containers and those which should not
      // be included.
      if (window->parent()) {
        if (!IsSwitchableContainer(window->parent()))
          continue;

        if (active_desk_only) {
          // If only the active desk's MRU windows are requested, then exclude
          // children of the non-active desks' containers.
          const int parent_id = window->parent()->id();
          if (desks_util::IsDeskContainerId(parent_id) &&
              parent_id != active_desk_id) {
            continue;
          }
        }

        if (!can_include_window_predicate(window))
          continue;
      }

      windows.emplace_back(window);
    }
  }

  auto roots = Shell::GetAllRootWindows();

  // Put the active root window last in |roots| so that when we iterate over the
  // root windows in reverse order below, the active root comes first. We do
  // this so that the top-most windows in the active root window will be added
  // first to |windows|.
  aura::Window* active_root = Shell::GetRootWindowForNewWindows();
  auto iter = std::find(roots.begin(), roots.end(), active_root);
  // When switching to/from Unified Mode, the active root window controller
  // might be in the process of shutting down, and its windows are being moved
  // to another root window before the root window for new windows is updated.
  // See WindowTreeHostManager::DeleteHost().
  if (iter != roots.end()) {
    roots.erase(iter);
    roots.emplace_back(active_root);
  }

  // TODO(afakhry): Check with UX, if kAllDesks is desired, should we put
  // the active desk's windows at the front?

  for (auto* root : base::Reversed(roots)) {
    // |wm::kSwitchableWindowContainerIds[]| contains a list of the container
    // IDs sorted such that the ID of the top-most container comes last. Hence,
    // we iterate in reverse order so the top-most windows are added first.
    const auto switachable_containers =
        GetSwitchableContainersForRoot(root, active_desk_only);
    for (auto* container : base::Reversed(switachable_containers)) {
      for (auto* child : base::Reversed(container->children())) {
        // Only add windows that the predicate allows.
        if (!can_include_window_predicate(child))
          continue;

        // Only add windows that have not been added previously from
        // |mru_windows| (if available).
        if (mru_windows && base::Contains(*mru_windows, child))
          continue;

        windows.emplace_back(child);
      }
    }
  }

  return windows;
}

}  // namespace

bool CanIncludeWindowInMruList(aura::Window* window) {
  return wm::CanActivateWindow(window) && !WindowState::Get(window)->IsPip();
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, public:

MruWindowTracker::MruWindowTracker() {
  Shell::Get()->activation_client()->AddObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  for (auto* window : mru_windows_)
    window->RemoveObserver(this);
}

MruWindowTracker::WindowList MruWindowTracker::BuildMruWindowList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInMruList);
}

MruWindowTracker::WindowList MruWindowTracker::BuildWindowListIgnoreModal(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(nullptr, desks_mru_type,
                                 IsWindowConsideredActivatable);
}

MruWindowTracker::WindowList MruWindowTracker::BuildWindowForCycleList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInCycleList);
}

MruWindowTracker::WindowList MruWindowTracker::BuildWindowForCycleWithPipList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInCycleWithPipList);
}

void MruWindowTracker::SetIgnoreActivations(bool ignore) {
  ignore_window_activations_ = ignore;

  // If no longer ignoring window activations, move currently active window
  // to front.
  if (!ignore)
    SetActiveWindow(window_util::GetActiveWindow());
}

void MruWindowTracker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MruWindowTracker::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, private:

void MruWindowTracker::SetActiveWindow(aura::Window* active_window) {
  if (!active_window)
    return;

  auto iter =
      std::find(mru_windows_.begin(), mru_windows_.end(), active_window);
  // Observe all newly tracked windows.
  if (iter == mru_windows_.end())
    active_window->AddObserver(this);
  else
    mru_windows_.erase(iter);
  mru_windows_.emplace_back(active_window);
}

void MruWindowTracker::OnWindowActivated(ActivationReason reason,
                                         aura::Window* gained_active,
                                         aura::Window* lost_active) {
  if (!ignore_window_activations_)
    SetActiveWindow(gained_active);
}

void MruWindowTracker::OnWindowDestroyed(aura::Window* window) {
  // It's possible for OnWindowActivated() to be called after
  // OnWindowDestroying(). This means we need to override OnWindowDestroyed()
  // else we may end up with a deleted window in |mru_windows_|.
  base::Erase(mru_windows_, window);
  window->RemoveObserver(this);

  for (auto& observer : observers_)
    observer.OnWindowUntracked(window);
}

}  // namespace ash
