// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
#define ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_

#include "ash/wm/overview/overview_session.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ui {
namespace test {
class EventGenerator;
}  // namespace test
}  // namespace ui

namespace ash {

void SendKey(ui::KeyboardCode key, int flags = ui::EF_NONE);

// Highlights |window| in the active overview session by cycling through all
// windows in overview until it is found. Returns true if |window| was found,
// false otherwise.
bool HighlightOverviewWindow(const aura::Window* window);

// Gets the current highlighted window. Returns nullptr if no window is
// highlighted.
const aura::Window* GetOverviewHighlightedWindow();

void ToggleOverview(
    OverviewEnterExitType type = OverviewEnterExitType::kNormal);

// Waits for the overview enter/exit animations to finish. No-op and immediately
// returns if animations are disabled.
void WaitForOverviewEnterAnimation();
void WaitForOverviewExitAnimation();

OverviewSession* GetOverviewSession();

OverviewGrid* GetOverviewGridForRoot(aura::Window* root);

const std::vector<std::unique_ptr<OverviewItem>>& GetOverviewItemsForRoot(
    int index);

// Returns the OverviewItem associated with |window| if it exists.
OverviewItem* GetOverviewItemForWindow(aura::Window* window);

// Returns a rect that accounts for the shelf hotseat. Used by tests which test
// the grids' bounds in relation to work area or snapped window bounds.
gfx::Rect ShrinkBoundsByHotseatInset(const gfx::Rect& rect);

// If `drop` is false, the dragged `item` won't be dropped; giving the caller
// a chance to do some validations before the item is dropped.
void DragItemToPoint(OverviewItem* item,
                     const gfx::Point& screen_location,
                     ui::test::EventGenerator* event_generator,
                     bool by_touch_gestures = false,
                     bool drop = true);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_TEST_UTIL_H_
