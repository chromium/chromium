// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_

#include <memory>
#include <vector>

namespace ash {

class DesksBarView;
class DeskMiniView;
class ExpandedStateNewDeskButton;

// Animates new desk mini_views, fading them in and moving them from right to
// left into their final positions in the desk bar view. It will also animate
// existing desks to show them moving as a result of creating the new
// mini_views.
// |bar_view| is the desk bar view on which the new mini_views are being
// added. |new_mini_views| contains a list of the newly-created mini_views.
// |shift_x| is the amount by which the mini_views (new and existing) will be
// moved horizontally as a result of creating the new mini_views.
//
// * Notes:
// - It assumes that the new mini_views have already been created, and all
//   mini_views (new and existing) have already been laid out in their final
//   positions.
// - If this is the first time mini_views are being created in the desk bar
//   view, it will also animate the bar background from top to buttom, and shift
//   the windows in the overview grid down.
void PerformNewDeskMiniViewAnimation(
    DesksBarView* bar_view,
    const std::vector<DeskMiniView*>& new_mini_views,
    int shift_x,
    bool first_time_mini_views);

// Performs the mini_view removal animation. It is in charge of removing the
// |removed_mini_view| from the views hierarchy and deleting it.
// |mini_views_left|, and |mini_views_right| are lists of the remaining
// mini_views to left and to the right of the removed mini_view respectively.
// |expanded_state_new_desk_button| will be moved to right the same as
// |mini_views_right| if Bento is enabled. Either list can be empty (e.g. if the
// removed mini_view is the last one on the right). |shift_x| is the amount by
// which the remaining mini_views will be moved horizontally to occupy the space
// that the removed mini_view used to occupy. It assumes that the remaining
// mini_views have been laid out in their final positions as if the removed
// mini_view no longer exists.
void PerformRemoveDeskMiniViewAnimation(
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    ExpandedStateNewDeskButton* expanded_state_new_desk_button,
    int shift_x);

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
