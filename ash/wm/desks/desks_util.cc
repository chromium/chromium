// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_util.h"

#include <array>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/containers/adapters.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace desks_util {

namespace {

constexpr std::array<int, kDesksUpperLimit> kDesksContainersIds = {
    kShellWindowId_DeskContainerA, kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC, kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE, kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG, kShellWindowId_DeskContainerH,
    kShellWindowId_DeskContainerI, kShellWindowId_DeskContainerJ,
    kShellWindowId_DeskContainerK, kShellWindowId_DeskContainerL,
    kShellWindowId_DeskContainerM, kShellWindowId_DeskContainerN,
    kShellWindowId_DeskContainerO, kShellWindowId_DeskContainerP,
};

// Default max number of desks (that is, enable-16-desks is off).
constexpr size_t kDesksDefaultLimit = 8;

}  // namespace

size_t GetMaxNumberOfDesks() {
  return features::Is16DesksEnabled() ? kDesksUpperLimit : kDesksDefaultLimit;
}

std::vector<int> GetDesksContainersIds() {
  return std::vector<int>(kDesksContainersIds.begin(),
                          kDesksContainersIds.begin() + GetMaxNumberOfDesks());
}

std::vector<aura::Window*> GetDesksContainers(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  std::vector<aura::Window*> containers;
  for (const auto& id : GetDesksContainersIds()) {
    auto* container = root->GetChildById(id);
    DCHECK(container);
    containers.push_back(container);
  }

  return containers;
}

const char* GetDeskContainerName(int container_id) {
  if (!IsDeskContainerId(container_id)) {
    NOTREACHED();
    return "";
  }

  static const char* kDeskContainerNames[] = {
      "Desk_Container_A", "Desk_Container_B", "Desk_Container_C",
      "Desk_Container_D", "Desk_Container_E", "Desk_Container_F",
      "Desk_Container_G", "Desk_Container_H", "Desk_Container_I",
      "Desk_Container_J", "Desk_Container_K", "Desk_Container_L",
      "Desk_Container_M", "Desk_Container_N", "Desk_Container_O",
      "Desk_Container_P",
  };
  return kDeskContainerNames[container_id - kShellWindowId_DeskContainerA];
}

bool IsDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return IsDeskContainerId(container->GetId());
}

bool IsDeskContainerId(int id) {
  return id >= kShellWindowId_DeskContainerA &&
         id <= kShellWindowId_DeskContainerP;
}

int GetActiveDeskContainerId() {
  auto* controller = DesksController::Get();
  DCHECK(controller);

  return controller->active_desk()->container_id();
}

ASH_EXPORT bool IsActiveDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return container->GetId() == GetActiveDeskContainerId();
}

aura::Window* GetActiveDeskContainerForRoot(aura::Window* root) {
  DCHECK(root);
  return root->GetChildById(GetActiveDeskContainerId());
}

ASH_EXPORT bool BelongsToActiveDesk(aura::Window* window) {
  DCHECK(window);

  // This function may be called early on during window construction. If there
  // is no parent, then it's not part of any desk yet. See b/260851890 for more
  // details.
  if (!window->parent())
    return false;

  auto* window_state = WindowState::Get(window);
  // A floated window may be associated with a desk, but they would be parented
  // to the float container.
  if (window_state && window_state->IsFloated()) {
    // When restoring floated window, this will be called when window is not
    // assigned to a desk by float controller yet. Only return `desk` when it
    // exists.
    // Note: in above case, `window` still belongs to desk container and
    // can be checked in statements below.
    if (auto* desk =
            Shell::Get()->float_controller()->FindDeskOfFloatedWindow(window)) {
      return desk->is_active();
    }
  }

  const int active_desk_id = GetActiveDeskContainerId();
  aura::Window* desk_container = GetDeskContainerForContext(window);
  return desk_container && desk_container->GetId() == active_desk_id;
}

aura::Window* GetDeskContainerForContext(aura::Window* context) {
  DCHECK(context);

  while (context) {
    if (IsDeskContainerId(context->GetId()))
      return context;

    context = context->parent();
  }

  return nullptr;
}

const Desk* GetDeskForContext(aura::Window* context) {
  DCHECK(context);

  for (const auto& desk : DesksController::Get()->desks()) {
    if (desk.get()->container_id() ==
        GetDeskContainerForContext(context)->GetId()) {
      return desk.get();
    }
  }

  if (WindowState::Get(context)->IsFloated())
    return Shell::Get()->float_controller()->FindDeskOfFloatedWindow(context);

  return nullptr;
}

bool ShouldDesksBarBeCreated() {
  return !TabletMode::Get()->InTabletMode() ||
         DesksController::Get()->desks().size() > 1;
}

ui::Compositor* GetSelectedCompositorForPerformanceMetrics() {
  // Favor the compositor associated with the active window's root window (if
  // any), or that of the primary root window.
  auto* active_window = window_util::GetActiveWindow();
  auto* selected_root = active_window && active_window->GetRootWindow()
                            ? active_window->GetRootWindow()
                            : Shell::GetPrimaryRootWindow();
  DCHECK(selected_root);
  return selected_root->layer()->GetCompositor();
}

bool IsDraggingAnyDesk() {
  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  if (!overview_session)
    return false;

  for (auto& grid : overview_session->grid_list()) {
    const DesksBarView* desks_bar_view = grid->desks_bar_view();
    if (desks_bar_view && desks_bar_view->IsDraggingDesk())
      return true;
  }

  return false;
}

bool IsWindowVisibleOnAllWorkspaces(const aura::Window* window) {
  return window->GetProperty(aura::client::kWindowWorkspaceKey) ==
         aura::client::kWindowWorkspaceVisibleOnAllWorkspaces;
}

bool IsZOrderTracked(aura::Window* window) {
  return window->GetType() == aura::client::WindowType::WINDOW_TYPE_NORMAL &&
         window->GetProperty(aura::client::kZOrderingKey) ==
             ui::ZOrderLevel::kNormal;
}

absl::optional<size_t> GetWindowZOrder(
    const std::vector<aura::Window*>& windows,
    aura::Window* window) {
  size_t position = 0;
  for (aura::Window* w : base::Reversed(windows)) {
    if (IsZOrderTracked(w)) {
      if (w == window)
        return position;
      ++position;
    }
  }

  return absl::nullopt;
}

}  // namespace desks_util

}  // namespace ash
