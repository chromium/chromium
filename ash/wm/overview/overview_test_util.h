// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_

#include "ash/wm/overview/overview_session.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui::test {
class EventGenerator;
}  // namespace ui::test

namespace ash {

class OverviewItemBase;

void SendKey(ui::KeyboardCode key, int flags = ui::EF_NONE);

// Focuses `window` in the active overview session by cycling through all
// windows in overview until it is found. Returns true if `window` was found,
// false otherwise.
bool FocusOverviewWindow(const aura::Window* window);

// Gets the current focused window. Returns nullptr if no window is focused.
const aura::Window* GetOverviewFocusedWindow();

void ToggleOverview(
    OverviewEnterExitType type = OverviewEnterExitType::kNormal);

// Waits for the overview enter/exit animations to finish. No-op and immediately
// returns if animations are disabled.
void WaitForOverviewEnterAnimation();
void WaitForOverviewExitAnimation();

OverviewGrid* GetOverviewGridForRoot(aura::Window* root);

const std::vector<std::unique_ptr<OverviewItemBase>>& GetOverviewItemsForRoot(
    int index);

std::vector<aura::Window*> GetWindowsListInOverviewGrids();

// Returns the OverviewItem associated with |window| if it exists.
OverviewItemBase* GetOverviewItemForWindow(aura::Window* window);

// Returns a rect that accounts for the shelf hotseat. Used by tests which test
// the grids' bounds in relation to work area or snapped window bounds.
gfx::Rect ShrinkBoundsByHotseatInset(const gfx::Rect& rect);

// If `drop` is false, the dragged `item` won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItemBase* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures = false,
                     bool drop = true);

// Press the key repeatedly until a window is focused, i.e. ignoring any
// desk items.
void SendKeyUntilOverviewItemIsFocused(ui::KeyboardCode key);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
