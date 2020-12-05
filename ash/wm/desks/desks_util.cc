// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_util.h"

#include <array>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"

namespace ash {

namespace desks_util {

namespace {

constexpr size_t kMaxNumberOfDesks = 4;
constexpr size_t kBentoMaxNumberOfDesks = 8;

constexpr std::array<int, kBentoMaxNumberOfDesks> kDesksContainersIds = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE,
    kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG,
    kShellWindowId_DeskContainerH,
};

}  // namespace

size_t GetMaxNumberOfDesks() {
  return features::IsBentoEnabled() ? kBentoMaxNumberOfDesks
                                    : kMaxNumberOfDesks;
}

std::vector<int> GetDesksContainersIds() {
  if (!features::IsBentoEnabled()) {
    return std::vector<int>(kDesksContainersIds.begin(),
                            kDesksContainersIds.begin() + kMaxNumberOfDesks);
  }
  return std::vector<int>(kDesksContainersIds.begin(),
                          kDesksContainersIds.end());
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

    default:
      NOTREACHED();
      return "";
  }
}

bool IsDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return IsDeskContainerId(container->id());
}

bool IsDeskContainerId(int id) {
  return id == kShellWindowId_DefaultContainerDeprecated ||
         id == kShellWindowId_DeskContainerB ||
         id == kShellWindowId_DeskContainerC ||
         id == kShellWindowId_DeskContainerD ||
         id == kShellWindowId_DeskContainerE ||
         id == kShellWindowId_DeskContainerF ||
         id == kShellWindowId_DeskContainerG ||
         id == kShellWindowId_DeskContainerH;
}

int GetActiveDeskContainerId() {
  auto* controller = DesksController::Get();
  DCHECK(controller);

  return controller->active_desk()->container_id();
}

ASH_EXPORT bool IsActiveDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return container->id() == GetActiveDeskContainerId();
}

aura::Window* GetActiveDeskContainerForRoot(aura::Window* root) {
  DCHECK(root);
  return root->GetChildById(GetActiveDeskContainerId());
}

ASH_EXPORT bool BelongsToActiveDesk(aura::Window* window) {
  DCHECK(window);

  const int active_desk_id = GetActiveDeskContainerId();
  aura::Window* desk_container = GetDeskContainerForContext(window);
  return desk_container && desk_container->id() == active_desk_id;
}

aura::Window* GetDeskContainerForContext(aura::Window* context) {
  DCHECK(context);

  while (context) {
    if (IsDeskContainerId(context->id()))
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

}  // namespace desks_util

}  // namespace ash
