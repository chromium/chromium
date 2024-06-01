// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_focus_rules.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "components/app_restore/full_restore_utils.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

bool BelongsToContainerWithEqualOrGreaterId(const aura::Window* window,
                                            int container_id) {
  for (; window; window = window->parent()) {
    if (window->GetId() >= container_id)
      return true;
  }
  return false;
}

bool BelongsToContainerWithId(const aura::Window* window, int container_id) {
  for (; window; window = window->parent()) {
    if (window->GetId() == container_id)
      return true;
  }
  return false;
}

bool IsInactiveDeskContainerId(int id) {
  return desks_util::IsDeskContainerId(id) &&
         id != desks_util::GetActiveDeskContainerId();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, public:

AshFocusRules::AshFocusRules()
    : activatable_container_ids_(GetActivatableShellWindowIds()) {}

AshFocusRules::~AshFocusRules() = default;

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, ::wm::FocusRules:

bool AshFocusRules::IsToplevelWindow(const aura::Window* window) const {
  DCHECK(window);
  // The window must be in a valid hierarchy.
  if (!window->GetRootWindow() || !window->parent())
    return false;

  // The window must exist within a container that supports activation.
  // The window cannot be blocked by a modal transient.
  return base::Contains(activatable_container_ids_, window->parent()->GetId());
}

bool AshFocusRules::SupportsChildActivation(const aura::Window* window) const {
  return base::Contains(activatable_container_ids_, window->GetId());
}

bool AshFocusRules::IsWindowConsideredVisibleForActivation(
    const aura::Window* window) const {
  DCHECK(window);

  Shell* shell = Shell::Get();
  // If the |window| doesn't belong to the current active user and also doesn't
  // show for the current active user, then it should not be activated.
  if (!shell->shell_delegate()->CanShowWindowForUser(window))
    return false;

  if (window->IsVisible())
    return true;

  const WindowState* window_state = WindowState::Get(window);
  // Minimized windows are hidden in their minimized state, but they can always
  // be activated.
  if (window_state->IsMinimized())
    return true;

  if (window_state->IsFloated()) {
    auto* float_controller = shell->float_controller();
    // Floated windows are hidden if they belong to inactive desks, but they can
    // always be activated.
    if (float_controller->FindDeskOfFloatedWindow(window) !=
        DesksController::Get()->active_desk()) {
      return true;
    }

    // Tucked windows are hidden offscreen, but they can be activated.
    if (float_controller->IsFloatedWindowTuckedForTablet(window)) {
      return true;
    }
  }

  if (!window->TargetVisibility())
    return false;

  const aura::Window* const parent = window->parent();
  return desks_util::IsDeskContainer(parent) ||
         parent->GetId() == kShellWindowId_LockScreenContainer;
}

bool AshFocusRules::CanActivateWindow(const aura::Window* window) const {
  // Clearing activation is always permissible.
  if (!window)
    return true;

  if (!WindowRestoreController::CanActivateRestoredWindow(window))
    return false;

  // Special case during Full Restore that prevents the app list from being
  // activated during tablet mode if the topmost window of any root window is a
  // Full Restore'd window. See http://crbug/1202923.
  if (!WindowRestoreController::CanActivateAppList(window))
    return false;

  if (!BaseFocusRules::CanActivateWindow(window))
    return false;

  // Special case to allow the login shelf to be activatable when the OOBE
  // modal is visible. See http://crbug/871184
  // TODO: remove this special case once login shelf is moved into a child
  // widget of the lock screen (https://crbug.com/767235).
  if (Shell::Get()->session_controller()->IsUserSessionBlocked() &&
      BelongsToContainerWithId(window, kShellWindowId_ShelfContainer)) {
    return true;
  }

  int modal_container_id = Shell::GetOpenSystemModalWindowContainerId();
  if (modal_container_id >= 0)
    return BelongsToContainerWithEqualOrGreaterId(window, modal_container_id);

  return true;
}

bool AshFocusRules::CanFocusWindow(const aura::Window* window,
                                   const ui::Event* event) const {
  if (!window)
    return true;

  if (event && (event->IsMouseEvent() || event->IsGestureEvent()) &&
      !window->GetProperty(aura::client::kActivateOnPointerKey)) {
    return false;
  }

  return BaseFocusRules::CanFocusWindow(window, event);
}

aura::Window* AshFocusRules::GetNextActivatableWindow(
    aura::Window* ignore) const {
  DCHECK(ignore);
  // If the window that lost activation should be ignored, no need to change
  // window activation.
  if (ignore->GetProperty(kIgnoreWindowActivationKey)) {
    return nullptr;
  }

  // If the window that just lost focus |ignore| has a transient parent, then
  // start from the container of that parent, otherwise start from the container
  // of the most-recently-used window. If the list of MRU windows is empty, then
  // start from the container of |ignore|.
  aura::Window* starting_window = nullptr;
  aura::Window* transient_parent = ::wm::GetTransientParent(ignore);
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  // It's possible for this to be called either on shutdown or when a session is
  // not yet active, so we need to check for the existence of the overview
  // controller.
  if (overview_controller && overview_controller->InOverviewSession() &&
      overview_controller->overview_session()->IsSavedDeskUiLosingActivation(
          ignore)) {
    starting_window =
        overview_controller->overview_session()->GetOverviewFocusWindow();
  } else if (transient_parent) {
    starting_window = transient_parent;
  } else {
    MruWindowTracker* mru = Shell::Get()->mru_window_tracker();
    aura::Window::Windows windows = mru->BuildMruWindowList(kActiveDesk);
    starting_window = windows.empty() ? ignore : windows[0].get();
  }
  DCHECK(starting_window);

  // Look for windows to focus in |starting_window|'s container. If none are
  // found, we look in all the containers in front of |starting_window|'s
  // container, then all behind.
  int starting_container_index = 0;
  aura::Window* root = starting_window->GetRootWindow();
  if (!root)
    root = Shell::GetRootWindowForNewWindows();
  const int container_count = activatable_container_ids_.size();
  for (int i = 0; i < container_count; i++) {
    aura::Window* container =
        Shell::GetContainer(root, activatable_container_ids_[i]);
    if (container && container->Contains(starting_window)) {
      starting_container_index = i;
      break;
    }
  }

  aura::Window* window = nullptr;
  for (int i = starting_container_index; !window && i < container_count; i++)
    window = GetTopmostWindowToActivateForContainerIndex(i, ignore, root);
  if (!window && starting_container_index > 0) {
    for (int i = starting_container_index - 1; !window && i >= 0; i--)
      window = GetTopmostWindowToActivateForContainerIndex(i, ignore, root);
  }
  if (window) {
    DCHECK(!window->GetProperty(kIgnoreWindowActivationKey));
  }
  return window;
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, private:

aura::Window* AshFocusRules::GetTopmostWindowToActivateForContainerIndex(
    int index,
    aura::Window* ignore,
    aura::Window* priority_root) const {
  const int container_id = activatable_container_ids_[index];
  // Inactive desk containers should be ignored, since windows in them should
  // never be returned as a next activatable window.
  if (IsInactiveDeskContainerId(container_id))
    return nullptr;
  aura::Window* window = nullptr;
  aura::Window::Windows containers =
      GetContainersForAllRootWindows(container_id, priority_root);
  // Favor the top-most window (if any) on `priority_root`, since
  // `GetContainersForAllRootWindows()` will put the container belonging to
  // `priority_root` first.
  for (aura::Window* container : containers) {
    window = GetTopmostWindowToActivateInContainer(container, ignore);
    if (window)
      return window;
  }
  return window;
}

aura::Window* AshFocusRules::GetTopmostWindowToActivateInContainer(
    aura::Window* container,
    aura::Window* ignore) const {
  for (aura::Window* child : base::Reversed(container->children())) {
    WindowState* window_state = WindowState::Get(child);
    // A floated window should not be activatable if it's hidden on an inactive
    // desk.
    if (child != ignore && window_state->CanActivate() &&
        !window_state->IsMinimized() &&
        !(window_state->IsFloated() && !child->IsVisible()) &&
        !child->GetProperty(kIgnoreWindowActivationKey)) {
      return child;
    }
  }
  return nullptr;
}

}  // namespace ash
