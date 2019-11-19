// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_focus_rules.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "base/stl_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

bool BelongsToContainerWithEqualOrGreaterId(const aura::Window* window,
                                            int container_id) {
  for (; window; window = window->parent()) {
    if (window->id() >= container_id)
      return true;
  }
  return false;
}

bool BelongsToContainerWithId(const aura::Window* window, int container_id) {
  for (; window; window = window->parent()) {
    if (window->id() == container_id)
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
  return base::Contains(activatable_container_ids_, window->parent()->id());
}

bool AshFocusRules::SupportsChildActivation(const aura::Window* window) const {
  return base::Contains(activatable_container_ids_, window->id());
}

bool AshFocusRules::IsWindowConsideredVisibleForActivation(
    const aura::Window* window) const {
  DCHECK(window);
  // If the |window| doesn't belong to the current active user and also doesn't
  // show for the current active user, then it should not be activated.
  if (!Shell::Get()->shell_delegate()->CanShowWindowForUser(window))
    return false;

  if (window->IsVisible())
    return true;

  // Minimized windows are hidden in their minimized state, but they can always
  // be activated.
  if (WindowState::Get(window)->IsMinimized())
    return true;

  if (!window->TargetVisibility())
    return false;

  const aura::Window* const parent = window->parent();
  return desks_util::IsDeskContainer(parent) ||
         parent->id() == kShellWindowId_LockScreenContainer;
}

bool AshFocusRules::CanActivateWindow(const aura::Window* window) const {
  // Clearing activation is always permissible.
  if (!window)
    return true;

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

  // If the window that just lost focus |ignore| has a transient parent, then
  // start from the container of that parent, otherwise start from the container
  // of the most-recently-used window. If the list of MRU windows is empty, then
  // start from the container of |ignore|.
  aura::Window* starting_window = nullptr;
  aura::Window* transient_parent = ::wm::GetTransientParent(ignore);
  if (transient_parent) {
    starting_window = transient_parent;
  } else {
    MruWindowTracker* mru = Shell::Get()->mru_window_tracker();
    aura::Window::Windows windows = mru->BuildMruWindowList(kActiveDesk);
    starting_window = windows.empty() ? ignore : windows[0];
  }
  DCHECK(starting_window);

  // Look for windows to focus in |starting_window|'s container. If none are
  // found, we look in all the containers in front of |starting_window|'s
  // container, then all behind.
  int starting_container_index = 0;
  aura::Window* root = starting_window->GetRootWindow();
  if (!root)
    root = Shell::GetRootWindowForNewWindows();
  int container_count = activatable_container_ids_.size();
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
    window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  if (!window && starting_container_index > 0) {
    for (int i = starting_container_index - 1; !window && i >= 0; i--)
      window = GetTopmostWindowToActivateForContainerIndex(i, ignore);
  }
  return window;
}

////////////////////////////////////////////////////////////////////////////////
// AshFocusRules, private:

aura::Window* AshFocusRules::GetTopmostWindowToActivateForContainerIndex(
    int index,
    aura::Window* ignore) const {
  const int container_id = activatable_container_ids_[index];
  // Inactive desk containers should be ignored, since windows in them should
  // never be returned as a next activatable window.
  if (IsInactiveDeskContainerId(container_id))
    return nullptr;
  aura::Window* window = nullptr;
  aura::Window* root = ignore ? ignore->GetRootWindow() : nullptr;
  aura::Window::Windows containers =
      GetContainersForAllRootWindows(container_id, root);
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
  for (aura::Window::Windows::const_reverse_iterator i =
           container->children().rbegin();
       i != container->children().rend(); ++i) {
    WindowState* window_state = WindowState::Get(*i);
    if (*i != ignore && window_state->CanActivate() &&
        !window_state->IsMinimized())
      return *i;
  }
  return nullptr;
}

}  // namespace ash
