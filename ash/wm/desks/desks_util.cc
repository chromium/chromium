// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desks_util.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/tablet_mode.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ui/aura/window.h"

namespace ash {

namespace desks_util {

namespace {

constexpr std::array<int, kMaxNumberOfDesks> kDesksContainersIds = {
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
};

}  // namespace

std::vector<int> GetDesksContainersIds() {
  if (!features::IsVirtualDesksEnabled())
    return std::vector<int>({kShellWindowId_DefaultContainerDeprecated});

  return std::vector<int>(kDesksContainersIds.begin(),
                          kDesksContainersIds.end());
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

    default:
      NOTREACHED();
      return "";
  }
}

std::vector<aura::Window*> GetDesksContainers(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  std::vector<aura::Window*> containers;
  for (const auto& id : GetDesksContainersIds()) {
    auto* container = root->GetChildById(id);
    DCHECK(container);
    containers.emplace_back(container);
  }

  return containers;
}

bool IsDeskContainer(const aura::Window* container) {
  DCHECK(container);
  return IsDeskContainerId(container->id());
}

bool IsDeskContainerId(int id) {
  return id == kShellWindowId_DefaultContainerDeprecated ||
         id == kShellWindowId_DeskContainerB ||
         id == kShellWindowId_DeskContainerC ||
         id == kShellWindowId_DeskContainerD;
}

int GetActiveDeskContainerId() {
  if (!features::IsVirtualDesksEnabled())
    return kShellWindowId_DefaultContainerDeprecated;

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
  return features::IsVirtualDesksEnabled() &&
         (!TabletMode::Get()->InTabletMode() ||
          DesksController::Get()->desks().size() > 1);
}

}  // namespace desks_util

}  // namespace ash
