// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"

#include <array>

#include "base/containers/contains.h"

namespace ash {

namespace {

// List of IDs of the containers whose windows are actiavated *before* windows
// in the desks containers.
constexpr std::array<int, 13> kPreDesksActivatableContainersIds = {
    kShellWindowId_OverlayContainer,
    kShellWindowId_LockSystemModalContainer,
    kShellWindowId_AccessibilityBubbleContainer,
    kShellWindowId_AccessibilityPanelContainer,
    kShellWindowId_SettingBubbleContainer,
    kShellWindowId_LiveCaptionContainer,
    kShellWindowId_PowerMenuContainer,
    kShellWindowId_LockActionHandlerContainer,
    kShellWindowId_LockScreenContainer,
    kShellWindowId_SystemModalContainer,
    kShellWindowId_AlwaysOnTopContainer,
    kShellWindowId_AppListContainer,
    kShellWindowId_HelpBubbleContainer,
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

// List of desk container IDs. Can't use desks_util since we're in ash/public
// here.
constexpr std::array<int, 16> kDeskContainerIds = {
    kShellWindowId_DeskContainerA, kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC, kShellWindowId_DeskContainerD,
    kShellWindowId_DeskContainerE, kShellWindowId_DeskContainerF,
    kShellWindowId_DeskContainerG, kShellWindowId_DeskContainerH,
    kShellWindowId_DeskContainerI, kShellWindowId_DeskContainerJ,
    kShellWindowId_DeskContainerK, kShellWindowId_DeskContainerL,
    kShellWindowId_DeskContainerM, kShellWindowId_DeskContainerN,
    kShellWindowId_DeskContainerO, kShellWindowId_DeskContainerP,
};

}  // namespace

std::vector<int> GetActivatableShellWindowIds() {
  std::vector<int> ids(kPreDesksActivatableContainersIds.begin(),
                       kPreDesksActivatableContainersIds.end());

  ids.insert(ids.end(), kDeskContainerIds.begin(), kDeskContainerIds.end());
  ids.insert(ids.end(), kPostDesksActivatableContainersIds.begin(),
             kPostDesksActivatableContainersIds.end());
  return ids;
}

bool IsActivatableShellWindowId(int id) {
  return base::Contains(GetActivatableShellWindowIds(), id);
}

}  // namespace ash
