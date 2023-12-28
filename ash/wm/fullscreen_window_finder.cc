// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/fullscreen_window_finder.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/adapters.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// Gets the parent switchable container of |context|.
aura::Window* GetSwitchableContainerForContext(aura::Window* context) {
  while (context && !IsSwitchableContainer(context))
    context = context->parent();

  return context;
}

// Returns the active window if it is a child of a switchable container, or
// nullptr otherwise.
aura::Window* GetActiveWindowInSwitchableContainer() {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window || !IsSwitchableContainer(active_window->parent()))
    return nullptr;

  return active_window;
}

// Given |container|, returns its topmost visible and positionable child.
aura::Window* GetTopMostWindowInContainer(aura::Window* container) {
  DCHECK(container);
  DCHECK(IsSwitchableContainer(container));

  for (aura::Window* child : base::Reversed(container->children())) {
    // `child` may be type `aura::client::WINDOW_TYPE_CONTROL` which has no
    // WindowState.
    if (WindowState::Get(child) &&
        WindowState::Get(child)->IsUserPositionable() &&
        child->layer()->GetTargetVisibility()) {
      return child;
    }
  }

  return nullptr;
}

// Given a |topmost_window|, returns it or one of its transient parents if the
// returned window is fullscreen or pinned. Otherwise, return nullptr.
aura::Window* FindFullscreenOrPinnedWindow(aura::Window* topmost_window) {
  while (topmost_window) {
    const WindowState* window_state = WindowState::Get(topmost_window);

    if (window_state->IsFullscreen() || window_state->IsPinned())
      return topmost_window;

    topmost_window = ::wm::GetTransientParent(topmost_window);
    if (topmost_window)
      topmost_window = topmost_window->GetToplevelWindow();
  }

  return nullptr;
}

}  // namespace

aura::Window* GetWindowForFullscreenModeForContext(aura::Window* context) {
  DCHECK(!context->IsRootWindow());

  // Get the active window on the same switchable container as that of
  // |context| if any.
  aura::Window* switchable_container_for_context =
      GetSwitchableContainerForContext(context);
  aura::Window* topmost_window = GetActiveWindowInSwitchableContainer();
  if (!topmost_window || (switchable_container_for_context !=
                          GetSwitchableContainerForContext(topmost_window))) {
    if (!switchable_container_for_context)
      return nullptr;

    // If there's no active window, then get the topmost child on the switchable
    // container of context.
    topmost_window =
        GetTopMostWindowInContainer(switchable_container_for_context);
  }

  return FindFullscreenOrPinnedWindow(topmost_window);
}

aura::Window* GetWindowForFullscreenModeInRoot(aura::Window* root) {
  DCHECK(root->IsRootWindow());

  // Get the active window on the same |root| if any.
  aura::Window* topmost_window = GetActiveWindowInSwitchableContainer();
  if (!topmost_window || topmost_window->GetRootWindow() != root) {
    // If there's no active window, then get the top most child of the active
    // desk container.
    topmost_window = GetTopMostWindowInContainer(
        desks_util::GetActiveDeskContainerForRoot(root));
  }

  return FindFullscreenOrPinnedWindow(topmost_window);
}

}  // namespace ash
