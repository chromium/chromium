// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_window_state_manager.h"

#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

// Reactivate the most recently unminimized window on the active desk so as not
// to activate any desk that has been explicitly navigated away from, nor
// activate any window that has been explicitly minimized.
void ActivateMruUnminimizedWindowOnActiveDesk() {
  MruWindowTracker::WindowList mru_windows(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(
          DesksMruType::kActiveDesk));
  for (auto* window : mru_windows) {
    if (WindowState::Get(window)->GetStateType() !=
        WindowStateType::kMinimized) {
      WindowState::Get(window)->Activate();
      return;
    }
  }
}

}  // namespace

WallpaperWindowStateManager::WallpaperWindowStateManager() = default;

WallpaperWindowStateManager::~WallpaperWindowStateManager() = default;

void WallpaperWindowStateManager::MinimizeInactiveWindows(
    const std::string& user_id_hash) {
  if (user_id_hash_window_list_map_.find(user_id_hash) ==
      user_id_hash_window_list_map_.end()) {
    user_id_hash_window_list_map_[user_id_hash] = std::set<aura::Window*>();
  }
  std::set<aura::Window*>* results =
      &user_id_hash_window_list_map_[user_id_hash];

  aura::Window* active_window = window_util::GetActiveWindow();
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);

  for (aura::Window::Windows::iterator iter = windows.begin();
       iter != windows.end(); ++iter) {
    // Ignore active window and minimized windows.
    if (*iter == active_window || WindowState::Get(*iter)->IsMinimized())
      continue;

    if (!(*iter)->HasObserver(this))
      (*iter)->AddObserver(this);

    results->insert(*iter);
    WindowState::Get(*iter)->Minimize();
  }
}

void WallpaperWindowStateManager::RestoreMinimizedWindows(
    const std::string& user_id_hash) {
  UserIDHashWindowListMap::iterator it =
      user_id_hash_window_list_map_.find(user_id_hash);
  if (it == user_id_hash_window_list_map_.end()) {
    DCHECK(false) << "This should only be called after calling "
                  << "MinimizeInactiveWindows.";
    return;
  }

  std::set<aura::Window*> removed_windows;
  removed_windows.swap(it->second);
  user_id_hash_window_list_map_.erase(it);

  for (std::set<aura::Window*>::iterator iter = removed_windows.begin();
       iter != removed_windows.end(); ++iter) {
    WindowState::Get(*iter)->Unminimize();
    RemoveObserverIfUnreferenced(*iter);
  }

  // If the wallpaper app is closed while the desktop is in overview mode,
  // do not activate any window, because doing so will disable overview mode.
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    ActivateMruUnminimizedWindowOnActiveDesk();
}

void WallpaperWindowStateManager::RemoveObserverIfUnreferenced(
    aura::Window* window) {
  for (UserIDHashWindowListMap::iterator iter =
           user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end(); ++iter) {
    if (iter->second.find(window) != iter->second.end())
      return;
  }
  // Remove observer if |window| is not observed by any users.
  window->RemoveObserver(this);
}

void WallpaperWindowStateManager::OnWindowDestroyed(aura::Window* window) {
  for (UserIDHashWindowListMap::iterator iter =
           user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end(); ++iter) {
    iter->second.erase(window);
  }
}

void WallpaperWindowStateManager::OnWindowStackingChanged(
    aura::Window* window) {
  // If user interacted with the |window| while wallpaper picker is opening,
  // removes the |window| from observed list.
  for (auto iter = user_id_hash_window_list_map_.begin();
       iter != user_id_hash_window_list_map_.end(); ++iter) {
    iter->second.erase(window);
  }
  window->RemoveObserver(this);
}

}  // namespace ash
