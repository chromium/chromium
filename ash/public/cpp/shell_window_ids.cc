// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"

#include <array>

#include "base/containers/contains.h"

namespace ash {

namespace {

// TODO(minch): Consolidate the below lists when we launch Bento.

// List of IDs of the containers whose windows are actiavated *before* windows
// in the desks containers.
constexpr std::array<int, 11> kPreDesksActivatableContainersIds = {
    kShellWindowId_OverlayContainer,
    kShellWindowId_LockSystemModalContainer,
    kShellWindowId_AccessibilityBubbleContainer,
    kShellWindowId_AccessibilityPanelContainer,
    kShellWindowId_SettingBubbleContainer,
    kShellWindowId_PowerMenuContainer,
    kShellWindowId_LockActionHandlerContainer,
    kShellWindowId_LockScreenContainer,
    kShellWindowId_SystemModalContainer,
    kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_AppListContainer,
};

// List of IDs of the containers whose windows are actiavated *after* windows in
// the desks containers.
constexpr std::array<int, 5> kPostDesksActivatableContainersIds = {
    kShellWindowId_HomeScreenContainer,

    // Launcher and status are intentionally checked after other containers
    // even though these layers are higher. The user expects their windows
    // to be focused before these elements.
    kShellWindowId_FloatContainer,
    kShellWindowId_PipContainer,
    kShellWindowId_ShelfContainer,
    kShellWindowId_ShelfBubbleContainer,
};

}  // namespace

std::vector<int> GetActivatableShellWindowIds() {
  std::vector<int> ids(kPreDesksActivatableContainersIds.begin(),
                       kPreDesksActivatableContainersIds.end());

  // Add the desks containers IDs. Can't use desks_util since we're in
  // ash/public here.
  ids.emplace_back(kShellWindowId_DefaultContainerDeprecated);
  ids.emplace_back(kShellWindowId_DeskContainerB);
  ids.emplace_back(kShellWindowId_DeskContainerC);
  ids.emplace_back(kShellWindowId_DeskContainerD);
  ids.emplace_back(kShellWindowId_DeskContainerE);
  ids.emplace_back(kShellWindowId_DeskContainerF);
  ids.emplace_back(kShellWindowId_DeskContainerG);
  ids.emplace_back(kShellWindowId_DeskContainerH);

  ids.insert(ids.end(), kPostDesksActivatableContainersIds.begin(),
             kPostDesksActivatableContainersIds.end());
  return ids;
}

bool IsActivatableShellWindowId(int id) {
  return base::Contains(GetActivatableShellWindowIds(), id);
}

}  // namespace ash
