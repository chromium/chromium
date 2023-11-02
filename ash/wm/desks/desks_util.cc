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
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace desks_util {

namespace {

constexpr std::array<int, kDesksUpperLimit> kDesksContainersIds = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE,
    kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG,
    kShellWindowId_DeskContainerH,
    kShellWindowId_DeskContainerI,
    kShellWindowId_DeskContainerJ,
    kShellWindowId_DeskContainerK,
    kShellWindowId_DeskContainerL,
    kShellWindowId_DeskContainerM,
    kShellWindowId_DeskContainerN,
    kShellWindowId_DeskContainerO,
    kShellWindowId_DeskContainerP,
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
  DCHECK(IsDeskContainerId(container_id));

  switch (container_id) {
    case kShellWindowId_DefaultContainerDeprecated:
      return "Desk_Container_A";

    case kShellWindowId_DeskContainerB:
      return "Desk_Container_B";

    case kShellWindowId_DeskContainerC:
      return "Desk_Container_C";

    case kShellWindowId_DeskContainerD:
      return "Desk_Container_D";

    case kShellWindowId_DeskContainerE:
      return "Desk_Container_E";

    case kShellWindowId_DeskContainerF:
      return "Desk_Container_F";

    case kShellWindowId_DeskContainerG:
      return "Desk_Container_G";

    case kShellWindowId_DeskContainerH:
      return "Desk_Container_H";

    case kShellWindowId_DeskContainerI:
      return "Desk_Container_I";

    case kShellWindowId_DeskContainerJ:
      return "Desk_Container_J";

    case kShellWindowId_DeskContainerK:
      return "Desk_Container_K";

    case kShellWindowId_DeskContainerL:
      return "Desk_Container_L";

    case kShellWindowId_DeskContainerM:
      return "Desk_Container_M";

    case kShellWindowId_DeskContainerN:
      return "Desk_Container_N";

    case kShellWindowId_DeskContainerO:
      return "Desk_Container_O";

    case kShellWindowId_DeskContainerP:
      return "Desk_Container_P";

    default:
      NOTREACHED();
      return "";
  }
}

bool IsDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return IsDeskContainerId(container->GetId());
}

bool IsDeskContainerId(int id) {
  return id == kShellWindowId_DefaultContainerDeprecated ||
         id == kShellWindowId_DeskContainerB ||
         id == kShellWindowId_DeskContainerC ||
         id == kShellWindowId_DeskContainerD ||
         id == kShellWindowId_DeskContainerE ||
         id == kShellWindowId_DeskContainerF ||
         id == kShellWindowId_DeskContainerG ||
         id == kShellWindowId_DeskContainerH ||
         id == kShellWindowId_DeskContainerI ||
         id == kShellWindowId_DeskContainerJ ||
         id == kShellWindowId_DeskContainerK ||
         id == kShellWindowId_DeskContainerL ||
         id == kShellWindowId_DeskContainerM ||
         id == kShellWindowId_DeskContainerN ||
         id == kShellWindowId_DeskContainerO ||
         id == kShellWindowId_DeskContainerP;
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

}  // namespace desks_util

}  // namespace ash
