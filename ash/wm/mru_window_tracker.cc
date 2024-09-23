// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/mru_window_tracker.h"

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/ash_focus_rules.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {

namespace {

using WindowList = MruWindowTracker::WindowList;

// A class that observes a window that should not be destroyed inside a certain
// scope. This class is added to investigate crbug.com/937381 to see if it's
// possible that a window is destroyed while building up the mru window list.
// TODO(crbug.com/41444457): Remove this class once we figure out the reason.
class ScopedWindowClosingObserver : public aura::WindowObserver {
 public:
  explicit ScopedWindowClosingObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }

  ScopedWindowClosingObserver(const ScopedWindowClosingObserver&) = delete;
  ScopedWindowClosingObserver& operator=(const ScopedWindowClosingObserver&) =
      delete;

  ~ScopedWindowClosingObserver() override {
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override { CHECK(false); }

 private:
  raw_ptr<aura::Window> window_;
};

bool IsNonSysModalWindowConsideredActivatable(aura::Window* window) {
  if (window->GetProperty(kExcludeInMruKey)) {
    return false;
  }

  if (window->GetProperty(kOverviewUiKey)) {
    return false;
  }

  ScopedWindowClosingObserver observer(window);
  AshFocusRules* focus_rules = Shell::Get()->focus_rules();

  // Exclude system modal because we only care about non systm modal windows.
  if (window->GetProperty(aura::client::kModalKey) ==
      ui::mojom::ModalType::kSystem) {
    return false;
  }

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
// built for alt-tab cycling, including one of the windows for Android PIP apps.
// For single-activity PIP, the PIP window is included in the list. (in the case
// of single-activity PIP, the PIP window is the same as the original window.)
// For multi-activity PIP, the non-PIP activity is included in the list.
// See the comment for |kPipOriginalWindowKey| for more detail.
bool CanIncludeWindowInCycleWithPipList(aura::Window* window) {
  return CanIncludeWindowInCycleList(window) ||
         (window_util::IsArcPipWindow(window) &&
          window->GetProperty(ash::kPipOriginalWindowKey));
}

// Returns a list of windows ordered by their usage recency such that the most
// recently used window is at the front of the list.
// If |mru_windows| is passed, these windows are moved to the front of the list.
// If |desks_mru_type| is `kAllDesks`, then all active and inactive desk
// containers will be considered, otherwise only the active desk container is
// considered.
// It uses the given |can_include_window_predicate| to determine whether to
// include a window in the returned list or not.
template <class CanIncludeWindowPredicate>
MruWindowTracker::WindowList BuildWindowListInternal(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>* mru_windows,
    DesksMruType desks_mru_type,
    CanIncludeWindowPredicate can_include_window_predicate) {
  MruWindowTracker::WindowList windows;

  const Desk* active_desk = DesksController::Get()->active_desk();
  const int active_desk_id = active_desk->container_id();
  const bool active_desk_only = desks_mru_type == kActiveDesk;
  // Put the windows in the mru_windows list at the head, if it's available.
  if (mru_windows) {
    // The |mru_windows| are sorted such that the most recent window comes last,
    // hence iterate in reverse order.
    for (aura::Window* window : base::Reversed(*mru_windows)) {
      // Exclude windows in non-switchable containers and those which should
      // not be included.
      if (window->parent()) {
        if (!IsSwitchableContainer(window->parent()))
          continue;
        if (active_desk_only) {
          // Floated windows are children of the Float container, rather than
          // desks containers, so they need to be handled separately.
          // TODO(crbug.com/1358580): Change to use `active_floated_window_`
          // when it's added.
          if (WindowState::Get(window)->IsFloated() &&
              Shell::Get()->float_controller()->FindDeskOfFloatedWindow(
                  window) != active_desk) {
            continue;
          }
          // If only the active desk's MRU windows are requested, then exclude
          // children of the non-active desks' containers.
          const int parent_id = window->parent()->GetId();
          if (desks_util::IsDeskContainerId(parent_id) &&
              parent_id != active_desk_id) {
            continue;
          }
        }

        if (!can_include_window_predicate(window))
          continue;

        DCHECK(!window->GetProperty(kExcludeInMruKey));
        windows.emplace_back(window);
      }
    }
  }

  auto roots = Shell::GetAllRootWindows();

  // Put the active root window last in |roots| so that when we iterate over the
  // root windows in reverse order below, the active root comes first. We do
  // this so that the top-most windows in the active root window will be added
  // first to |windows|.
  aura::Window* active_root = Shell::GetRootWindowForNewWindows();
  auto iter = base::ranges::find(roots, active_root);
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

  for (aura::Window* root : base::Reversed(roots)) {
    // |wm::kSwitchableWindowContainerIds[]| contains a list of the container
    // IDs sorted such that the ID of the top-most container comes last. Hence,
    // we iterate in reverse order so the top-most windows are added first.
    const auto switachable_containers =
        GetSwitchableContainersForRoot(root, active_desk_only);
    for (auto* container : base::Reversed(switachable_containers)) {
      for (aura::Window* child : base::Reversed(container->children())) {
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
  // If `window` was launched from Full Restore it won't be activatable
  // temporarily, but it should still be included in the MRU list.
  if (window->GetProperty(app_restore::kLaunchedFromAppRestoreKey))
    return true;

  return wm::CanActivateWindow(window) &&
         !window->GetProperty(kExcludeInMruKey) &&
         !window->GetProperty(kOverviewUiKey);
}

bool CanIncludeWindowInAppMruList(aura::Window* window) {
  return window->GetProperty(chromeos::kAppTypeKey) !=
         chromeos::AppType::NON_APP;
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, public:

MruWindowTracker::MruWindowTracker() {
  Shell::Get()->activation_client()->AddObserver(this);
}

MruWindowTracker::~MruWindowTracker() {
  Shell::Get()->activation_client()->RemoveObserver(this);
  for (aura::Window* window : mru_windows_) {
    window->RemoveObserver(this);
  }
}

WindowList MruWindowTracker::BuildAppWindowList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInAppMruList);
}

WindowList MruWindowTracker::BuildMruWindowList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInMruList);
}

WindowList MruWindowTracker::BuildWindowListIgnoreModal(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 IsNonSysModalWindowConsideredActivatable);
}

WindowList MruWindowTracker::BuildWindowForCycleList(
    DesksMruType desks_mru_type) const {
  return BuildWindowListInternal(&mru_windows_, desks_mru_type,
                                 CanIncludeWindowInCycleList);
}

WindowList MruWindowTracker::BuildWindowForCycleWithPipList(
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

void MruWindowTracker::OnWindowMovedOutFromRemovingDesk(aura::Window* window) {
  DCHECK(window);

  auto iter = base::ranges::find(mru_windows_, window);
  if (iter != mru_windows_.end()) {
    mru_windows_.erase(iter);
    mru_windows_.insert(mru_windows_.begin(), window);
  }
}

void MruWindowTracker::OnWindowAlteredByWindowRestore(aura::Window* window) {
  int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  DCHECK(activation_index);

  // A window may shift desks and get restacked but already be created. In this
  // case remove it from `mru_windows_` and reinsert it at the correct location.
  // If nothing was erased, this is a window not currently observed so we want
  // to observe it as windows created from window restore aren't activated on
  // creation.
  size_t num_erased = std::erase(mru_windows_, window);
  if (num_erased == 0u)
    window->AddObserver(this);

  // When windows are restored from a window restore feature, they are restored
  // inactive so we have to manually insert them into the window tracker and
  // restore their MRU order.
  mru_windows_.insert(
      WindowRestoreController::GetWindowToInsertBefore(window, mru_windows_),
      window);
}

//////////////////////////////////////////////////////////////////////////////
// MruWindowTracker, private:

void MruWindowTracker::SetActiveWindow(aura::Window* active_window) {
  if (!active_window)
    return;

  auto iter = base::ranges::find(mru_windows_, active_window);
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
  if (ignore_window_activations_)
    return;

  SetActiveWindow(gained_active);

  if (gained_active)
    WindowRestoreController::Get()->OnWindowActivated(gained_active);
}

void MruWindowTracker::OnWindowDestroyed(aura::Window* window) {
  // It's possible for OnWindowActivated() to be called after
  // OnWindowDestroying(). This means we need to override OnWindowDestroyed()
  // else we may end up with a deleted window in |mru_windows_|.
  std::erase(mru_windows_, window);
  window->RemoveObserver(this);
}

}  // namespace ash
