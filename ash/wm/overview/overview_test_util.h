// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_

#include "ash/wm/overview/overview_session.h"
#include "base/macros.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

void SendKey(ui::KeyboardCode key, int flags = ui::EF_NONE);

// Highlights |window| in the active overview session by cycling through all
// windows in overview until it is found. Returns true if |window| was found,
// false otherwise.
bool HighlightOverviewWindow(const aura::Window* window);

// Gets the current highlighted window. Returns nullptr if no window is
// highlighted.
const aura::Window* GetOverviewHighlightedWindow();

void ToggleOverview(OverviewSession::EnterExitOverviewType type =
                        OverviewSession::EnterExitOverviewType::kNormal);

OverviewSession* GetOverviewSession();

const std::vector<std::unique_ptr<OverviewItem>>& GetOverviewItemsForRoot(
    int index);

// Returns the OverviewItem associated with |window| if it exists.
OverviewItem* GetOverviewItemForWindow(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
