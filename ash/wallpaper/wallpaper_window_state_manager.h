// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_WINDOW_STATE_MANAGER_H_
#define ASH_WALLPAPER_WALLPAPER_WINDOW_STATE_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ash {

// WallpaperWindowStateManager remembers which windows have been minimized in
// order to restore them when the wallpaper viewer is hidden.
class ASH_EXPORT WallpaperWindowStateManager : public aura::WindowObserver {
 public:
  typedef std::map<std::string,
                   std::set<raw_ptr<aura::Window, SetExperimental>>>
      UserIDHashWindowListMap;

  WallpaperWindowStateManager();

  WallpaperWindowStateManager(const WallpaperWindowStateManager&) = delete;
  WallpaperWindowStateManager& operator=(const WallpaperWindowStateManager&) =
      delete;

  ~WallpaperWindowStateManager() override;

  // Store all unminimized windows except |active_window| and minimize them.
  // All the windows are saved in a map and the key value is |user_id_hash|.
  void MinimizeInactiveWindows(const std::string& user_id_hash);

  // Unminimize all the stored windows for |user_id_hash|. This should only be
  // called after calling MinimizeInactiveWindows.
  void RestoreMinimizedWindows(const std::string& user_id_hash);

 private:
  // Remove the observer from |window| if |window| is no longer referenced in
  // user_id_hash_window_list_map_.
  void RemoveObserverIfUnreferenced(aura::Window* window);

  // aura::WindowObserver overrides.
  void OnWindowDestroyed(aura::Window* window) override;

  // aura::WindowObserver overrides.
  void OnWindowStackingChanged(aura::Window* window) override;

  // Map of user id hash and associated list of minimized windows.
  UserIDHashWindowListMap user_id_hash_window_list_map_;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_WINDOW_STATE_MANAGER_H_
