// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/always_on_top_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(bool, kDisallowReparentKey, false)

AlwaysOnTopController::AlwaysOnTopController(
    aura::Window* always_on_top_container,
    aura::Window* pip_container)
    : always_on_top_container_(always_on_top_container),
      pip_container_(pip_container) {
  DCHECK(!desks_util::IsDeskContainer(always_on_top_container_));
  DCHECK(!desks_util::IsDeskContainer(pip_container_));
  always_on_top_container_->SetLayoutManager(
      std::make_unique<WorkspaceLayoutManager>(always_on_top_container_));
  pip_container_->SetLayoutManager(
      std::make_unique<WorkspaceLayoutManager>(pip_container_));
  // Container should be empty.
  DCHECK(always_on_top_container_->children().empty());
  DCHECK(pip_container_->children().empty());
  always_on_top_container_->AddObserver(this);
  pip_container->AddObserver(this);
}

AlwaysOnTopController::~AlwaysOnTopController() {
  // At this point, all windows should be removed and AlwaysOnTopController
  // will have removed itself as an observer in OnWindowDestroying.
  DCHECK(!always_on_top_container_);
  DCHECK(!pip_container_);
}

// static
void AlwaysOnTopController::SetDisallowReparent(aura::Window* window) {
  window->SetProperty(kDisallowReparentKey, true);
}

aura::Window* AlwaysOnTopController::GetContainer(aura::Window* window) const {
  DCHECK(always_on_top_container_);
  DCHECK(pip_container_);

  // On other platforms, there are different window levels. For now, treat any
  // window with non-normal level as "always on top". Perhaps the nuance of
  // multiple levels will be needed later.
  if (window->GetProperty(aura::client::kZOrderingKey) ==
      ui::ZOrderLevel::kNormal) {
    aura::Window* root = always_on_top_container_->GetRootWindow();

    DesksController* desks_controller = DesksController::Get();
    const std::string* desk_uuid_string =
        window->GetProperty(aura::client::kDeskUuidKey);
    if (desk_uuid_string) {
      const base::Uuid desk_guid =
          base::Uuid::ParseLowercase(*desk_uuid_string);
      if (desk_guid.is_valid()) {
        if (Desk* target_desk = desks_controller->GetDeskByUuid(desk_guid)) {
          if (auto* container = target_desk->GetDeskContainerForRoot(root)) {
            return container;
          }
        }
      }
    }

    const int window_workspace =
        window->GetProperty(aura::client::kWindowWorkspaceKey);
    if (window_workspace != aura::client::kWindowWorkspaceUnassignedWorkspace) {
      auto* desk_container =
          desks_controller->GetDeskContainer(root, window_workspace);
      if (desk_container)
        return desk_container;
    }
    return desks_util::GetActiveDeskContainerForRoot(root);
  }
  if (window->parent() && WindowState::Get(window)->IsPip())
    return pip_container_;

  return always_on_top_container_;
}

void AlwaysOnTopController::ClearLayoutManagers() {
  always_on_top_container_->SetLayoutManager(nullptr);
  pip_container_->SetLayoutManager(nullptr);
}

void AlwaysOnTopController::SetLayoutManagerForTest(
    std::unique_ptr<WorkspaceLayoutManager> layout_manager) {
  always_on_top_container_->SetLayoutManager(std::move(layout_manager));
}

void AlwaysOnTopController::AddWindow(aura::Window* window) {
  window->AddObserver(this);
  WindowState::Get(window)->AddObserver(this);
}

void AlwaysOnTopController::RemoveWindow(aura::Window* window) {
  window->RemoveObserver(this);
  WindowState::Get(window)->RemoveObserver(this);
}

void AlwaysOnTopController::ReparentWindow(aura::Window* window) {
  DCHECK(window->GetType() == aura::client::WINDOW_TYPE_NORMAL ||
         window->GetType() == aura::client::WINDOW_TYPE_POPUP);
  aura::Window* container = GetContainer(window);
  if (window->parent() != container &&
      !window->GetProperty(kDisallowReparentKey))
    container->AddChild(window);
}

void AlwaysOnTopController::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  if (params.old_parent == always_on_top_container_.get() ||
      params.old_parent == pip_container_.get()) {
    RemoveWindow(params.target);
  }

  if (params.new_parent == always_on_top_container_.get() ||
      params.new_parent == pip_container_.get()) {
    AddWindow(params.target);
  }
}

void AlwaysOnTopController::OnWindowPropertyChanged(aura::Window* window,
                                                    const void* key,
                                                    intptr_t old) {
  if (window != always_on_top_container_ && window != pip_container_ &&
      key == aura::client::kZOrderingKey) {
    ReparentWindow(window);
  }
}

void AlwaysOnTopController::OnWindowDestroying(aura::Window* window) {
  if (window == always_on_top_container_) {
    always_on_top_container_->RemoveObserver(this);
    always_on_top_container_ = nullptr;
  } else if (window == pip_container_) {
    pip_container_->RemoveObserver(this);
    pip_container_ = nullptr;
  } else {
    RemoveWindow(window);
  }
}

void AlwaysOnTopController::OnPreWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  ReparentWindow(window_state->window());
}

}  // namespace ash
