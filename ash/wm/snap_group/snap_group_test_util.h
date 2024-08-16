// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/event_generator.h"

#ifndef ASH_WM_SNAP_GROUP_SNAP_GROUP_TEST_UTIL_H_
#define ASH_WM_SNAP_GROUP_SNAP_GROUP_TEST_UTIL_H_

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class SnapGroup;
class SplitViewDivider;

SplitViewDivider* GetTopmostSnapGroupDivider();

gfx::Rect GetTopmostSnapGroupDividerBoundsInScreen();

void ClickOverviewItem(ui::test::EventGenerator* event_generator,
                       aura::Window* window);

void SnapTwoTestWindows(aura::Window* window1,
                        aura::Window* window2,
                        bool horizontal,
                        ui::test::EventGenerator* event_generator);

// Verifies that the union bounds of `w1`, `w2` and the divider are equal to
// the bounds of the work area with no overlap.
void UnionBoundsEqualToWorkAreaBounds(aura::Window* w1,
                                      aura::Window* w2,
                                      SplitViewDivider* divider);

// Verifies that the union bounds of the windows and divider in `snap_group` are
// equal to the bounds of the work area with no overlap.
void UnionBoundsEqualToWorkAreaBounds(SnapGroup* snap_group);

}  // namespace ash

#endif  // ASH_WM_SNAP_GROUP_SNAP_GROUP_TEST_UTIL_H_
