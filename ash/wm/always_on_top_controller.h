// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_
#define ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_state_observer.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"

namespace ash {

class WorkspaceLayoutManager;

// AlwaysOnTopController puts window into proper containers based on its
// 'AlwaysOnTop' property. That is, putting a window into the worskpace
// container if its "AlwaysOnTop" property is false. Otherwise, put it in
// |always_on_top_container_|.
class ASH_EXPORT AlwaysOnTopController : public aura::WindowObserver,
                                         WindowStateObserver {
 public:
  explicit AlwaysOnTopController(aura::Window* always_on_top_container,
                                 aura::Window* pip_container);

  AlwaysOnTopController(const AlwaysOnTopController&) = delete;
  AlwaysOnTopController& operator=(const AlwaysOnTopController&) = delete;

  ~AlwaysOnTopController() override;

  static void SetDisallowReparent(aura::Window* window);

  // Gets container for given |window| based on its "AlwaysOnTop" property.
  aura::Window* GetContainer(aura::Window* window) const;

  // Clears the layout managers for |always_on_top_container_| and
  // |pip_container_|. This should only be called when the RootWindowController
  // is shutting down, to prevent the layout managers from doing unnecessary and
  // complex work.
  void ClearLayoutManagers();

  void SetLayoutManagerForTest(
      std::unique_ptr<WorkspaceLayoutManager> layout_manager);

 private:
  void AddWindow(aura::Window* window);
  void RemoveWindow(aura::Window* window);
  void ReparentWindow(aura::Window* window);

  // Overridden from aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from WindowStateObserver:
  void OnPreWindowStateTypeChange(WindowState* window_state,
                                  chromeos::WindowStateType old_type) override;

  raw_ptr<aura::Window> always_on_top_container_;
  raw_ptr<aura::Window> pip_container_;
};

}  // namespace ash

#endif  // ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_
