// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_

#include <memory>
#include <vector>
#include "base/containers/flat_map.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Transform;
}  // namespace gfx

namespace views {
class View;
class Widget;
}  // namespace views

namespace ash {

class CrOSNextDeskIconButton;
class DeskBarViewBase;
class DeskMiniView;
class ExpandedDesksBarButton;

// When deleting a desk on overview desk bar, this performs a fade out animation
// on `removed_mini_view`'s layer by changing its opacity from 1 to 0 and
// scaling it down around the center of desk bar view while switching back to
// zero state. `removed_mini_view` will be deleted when the animation is
// complete or aborted.
void PerformRemoveDeskMiniViewAnimation(DeskMiniView* removed_mini_view,
                                        const bool to_zero_state);

// Performs the animation on desk mini view when adding a new desk.
// `new_mini_views` contains a list of the newly-created mini_views. `shift_x`
// is the amount by which the mini_views will be moved horizontally as a result
// of creating the new mini_views. With Jelly disabled, the newly added mini
// view will shift with `shift_x` and fade in. When Jelly is enabled, the new
// mini view will scale up and fade in.
// * Notes:
// - It assumes that the new_mini_views have already been created, and all
// mini_views have already been laid out in their final positions.
void PerformAddDeskMiniViewAnimation(std::vector<DeskMiniView*> new_mini_views,
                                     int shift_x);

// Performs individual animation for views that belong to `bar_view` during
// desk adding/removing. On both overview and desk button bar, on desk
// removal, all other views (mini views, library button etc.) animate towards
// filling out the empty space created by removed desk mini view. On desk
// addition, all other views move away from newly added desk view to make space.
// `views_previous_x_map` is passed here with all the previous x locations of
// each view to calculate the individual transform.
void PerformDeskBarChildViewShiftAnimation(
    DeskBarViewBase* bar_view,
    const base::flat_map<views::View*, int>& views_previous_x_map);

// Performs the animation of switching from zero state desk bar to expanded
// state desk bar. It scales up and fades in the current mini views and the
// ExpandedDesksBarButton. Also animates the desk bar view from the zero state
// bar's height to the expanded bar's height.
void PerformZeroStateToExpandedStateMiniViewAnimation(
    DeskBarViewBase* bar_view);

void PerformZeroStateToExpandedStateMiniViewAnimationCrOSNext(
    DeskBarViewBase* bar_view);

// Performs the animation of switching from expanded state desk bar to zero
// state desk bar. This happens when a desk is removed such that a single desk
// is remaining. It scales down and fades out the `removed_mini_views` and the
// ExpandedDesksBarButton. `removed_mini_views` will be removed from the
// views hierarchy. But the ExpandedDesksBarButton will be kept and set to
// invisible. It will also animate the desk bar view from the expanded bar's
// height to zero state bar's height.
//
// * Notes:
// - It assumes `removed_mini_views` and the ExpandedDesksBarButton are still
//   laid out at their previous positions before the bar state transition.
// - Layout will be done once the animation is completed.
void PerformExpandedStateToZeroStateMiniViewAnimation(
    DeskBarViewBase* bar_view,
    std::vector<DeskMiniView*> removed_mini_views);

// Performs the animation for desk bar when desk is added. Desk bar will expand
// during animation.
void PerformDeskBarAddDeskAnimation(DeskBarViewBase* bar_view,
                                    const gfx::Rect& old_bar_bounds);
// Performs the animation for desk bar when desk is removed. Desk bar will
// shrink during animation.
void PerformDeskBarRemoveDeskAnimation(DeskBarViewBase* bar_view,
                                       const gfx::Rect& old_background_bounds);

// Performs the mini_view reorder animation. It moves the desks to make space at
// `new_index` for the mini_view at `old_index`. Before reordering, if
// `old_index` < `new_index`, the mini views from `old_index` + 1 to
// `new_index` would move left; if `old_index` > `new_index`, the mini
// views from `new_index` to `old_index` - 1 would move right.
//
// Note that the `mini_views` is the reordered list. Therefore, the range of the
// mini views to be moved should be selected according to the current position.
// If `old_index` < `new_index`, the range is from `old_index` to
// `new_index` - 1; otherwise, the range is from `new_index` + 1 to
// `old_index`.
void PerformReorderDeskMiniViewAnimation(
    int old_index,
    int new_index,
    const std::vector<DeskMiniView*>& mini_views);

// Performs the animation which happens when the saved desk library button is
// shown or hidden. Shifts all the mini views and the new desk button to the
// left by `shift_x`.
// * Notes:
// - It assumes all the `mini_views` and new desk button have been laid out in
//   their final positions.
void PerformLibraryButtonVisibilityAnimation(
    const std::vector<DeskMiniView*>& mini_views,
    views::View* new_desk_button,
    int shift_x);

// Performs the scale animation to the given `button` based on the given
// arguments. It also shifts the mini views, new desk button or library button
// by `shift_x` with animation.
// * Notes:
// - It assumes all the mini views in `bar_view`, new desk button and library
// button have been laid out in their final positions.
void PerformDeskIconButtonScaleAnimationCrOSNext(
    CrOSNextDeskIconButton* button,
    DeskBarViewBase* bar_view,
    const gfx::Transform& new_desk_button_rects_transform,
    int shift_x);

// Performs the slide out animation for `bar_view` when exiting overview. Please
// note, unlike other animations where we animate directly on the objects using
// `AnimationBuilder` we also pass ownership using `CleanupAnimationObserver`,
// which does not support abort handle.
void PerformDeskBarSlideAnimation(std::unique_ptr<views::Widget> desks_widget,
                                  bool is_zero_state);

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
