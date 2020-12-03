// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shell_window_ids.h"

#include "ash/public/cpp/ash_features.h"
#include "base/stl_util.h"

namespace ash {

namespace {

constexpr std::array<int, 19> kActivatableContainersIds = {
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
    kShellWindowId_DefaultContainerDeprecated,
    kShellWindowId_DeskContainerB,
    kShellWindowId_DeskContainerC,
    kShellWindowId_DeskContainerD,
    kShellWindowId_HomeScreenContainer,

    // Launcher and status are intentionally checked after other containers
    // even though these layers are higher. The user expects their windows
    // to be focused before these elements.
    kShellWindowId_PipContainer,
    kShellWindowId_ShelfContainer,
    kShellWindowId_ShelfBubbleContainer,
};

}  // namespace

// Note: this function avoids having a copy of |kActivatableContainersIds| in
// each translation unit that references it.
const std::array<int, 19>& GetActivatableShellWindowIds() {
  return kActivatableContainersIds;
}

bool IsActivatableShellWindowId(int id) {
  return base::Contains(kActivatableContainersIds, id);
}

}  // namespace ash
