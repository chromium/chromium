// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

class WorkspaceLayoutManager;

// WorkspaceController acts as a central place that ties together all the
// various workspace pieces.
class ASH_EXPORT WorkspaceController : public aura::WindowObserver {
 public:
  // Installs WorkspaceLayoutManager on |viewport|.
  explicit WorkspaceController(aura::Window* viewport);

  WorkspaceController(const WorkspaceController&) = delete;
  WorkspaceController& operator=(const WorkspaceController&) = delete;

  ~WorkspaceController() override;

  WorkspaceLayoutManager* layout_manager();

  // Returns the current window state.
  WorkspaceWindowState GetWindowState() const;

  // Starts the animation that occurs on first login.
  void DoInitialAnimation();

 private:
  friend class WorkspaceControllerTestApi;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  raw_ptr<aura::Window> viewport_;
  std::unique_ptr<WorkspaceEventHandler> event_handler_;
};

// Sets the given |workspace_controller| as a property of |desk_container|. Only
// virtual desks containers are accepted. If |workspace_controller| is nullptr,
// the property will be cleared from |desk_container|.
ASH_EXPORT void SetWorkspaceController(
    aura::Window* desk_container,
    std::unique_ptr<WorkspaceController> workspace_controller);

// Gets the worspace controller from the properties of the specific given
// |desk_container|. Only virtual desks containers are accepted.
ASH_EXPORT WorkspaceController* GetWorkspaceController(
    aura::Window* desk_container);

// Gets the workspace controller from the properties of the virtual desk
// container anscestor of |context|. Returns nullptr if |context| doesn't belong
// to any virtual desk.
ASH_EXPORT WorkspaceController* GetWorkspaceControllerForContext(
    aura::Window* context);

// Gets the workspace controller from the properties of the currently active
// virtual desk container on the given |root|.
ASH_EXPORT WorkspaceController* GetActiveWorkspaceController(
    aura::Window* root);

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_CONTROLLER_H_
