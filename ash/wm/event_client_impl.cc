// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/event_client_impl.h"

#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/tray_action/tray_action.h"
#include "ui/aura/window.h"

namespace ash {

EventClientImpl::EventClientImpl() = default;

EventClientImpl::~EventClientImpl() = default;

bool EventClientImpl::GetCanProcessEventsWithinSubtree(
    const aura::Window* window) const {
  if (skip_user_session_blocked_check_)
    return true;

  // TODO(oshima): Migrate this logic to Shell::CanWindowReceieveEvents and
  // remove this.
  const aura::Window* root_window = window ? window->GetRootWindow() : NULL;
  if (!root_window ||
      !Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return true;
  }

  const aura::Window* lock_screen_containers = Shell::GetContainer(
      root_window, kShellWindowId_LockScreenContainersContainer);
  const aura::Window* lock_wallpaper_containers = Shell::GetContainer(
      root_window, kShellWindowId_LockScreenWallpaperContainer);
  const aura::Window* lock_screen_related_containers = Shell::GetContainer(
      root_window, kShellWindowId_LockScreenRelatedContainersContainer);
  bool can_process_events =
      (window->Contains(lock_screen_containers) &&
       window->Contains(lock_wallpaper_containers) &&
       window->Contains(lock_screen_related_containers)) ||
      lock_screen_containers->Contains(window) ||
      lock_wallpaper_containers->Contains(window) ||
      lock_screen_related_containers->Contains(window);
  if (keyboard::IsKeyboardEnabled()) {
    const aura::Window* virtual_keyboard_container = Shell::GetContainer(
        root_window, kShellWindowId_VirtualKeyboardContainer);
    can_process_events |= (window->Contains(virtual_keyboard_container) ||
                           virtual_keyboard_container->Contains(window));
  }
  return can_process_events;
}

ui::EventTarget* EventClientImpl::GetToplevelEventTarget() {
  return Shell::Get();
}

}  // namespace ash
