// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
#define ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_

#include <memory>
#include <vector>

namespace gfx {
class Transform;
}  // namespace gfx

namespace views {
class View;
}  // namespace views

namespace ash {

class CrOSNextDeskIconButton;
class DeskBarViewBase;
class DeskMiniView;
class ExpandedDesksBarButton;

// Animates new desk mini_views, fading them into their final positions in the
// desk bar view. It will also animate existing desks to show them moving as a
// result of creating the new mini_views. `new_mini_views` contains a list of
// the newly-created mini_views. `mini_views_left` are the mini views on the
// left of the new mini views in the desk bar, while `mini_views_right` are the
// mini views on the right side of the new mini views.
// The new desk button and the library button (if it exists) will be moved to
// the right. `shift_x` is the amount by which the mini_views (new and existing)
// will be moved horizontally as a result of creating the new mini_views.
//
// * Notes:
// - It assumes that the new mini_views have already been created, and all
//   mini_views (new and existing) have already been laid out in their final
//   positions.
void PerformNewDeskMiniViewAnimation(
    DeskBarViewBase* bar_view,
    std::vector<DeskMiniView*> new_mini_views,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x);

// Performs the mini_view removal animation. It is in charge of removing the
// `removed_mini_view` from the views hierarchy and deleting it. We also update
// the `bar_view` desk buttons visibility once the animation completes.
// `mini_views_left`, and `mini_views_right` are lists of the remaining
// mini_views to left and to the right of the removed mini_view respectively.
// The new desk button will be moved to right the same as `mini_views_right`. If
// the library button is non-null, it will also be moved to the right the same
// as `mini_views_right`. Either list can be empty (e.g. if the removed
// mini_view is the last one on the right). `shift_x` is the amount by which the
// remaining mini_views will be moved horizontally to occupy the space that the
// removed mini_view used to occupy. It assumes that the remaining mini_views
// have been laid out in their final positions as if the removed mini_view no
// longer exists.
void PerformRemoveDeskMiniViewAnimation(
    DeskBarViewBase* bar_view,
    DeskMiniView* removed_mini_view,
    std::vector<DeskMiniView*> mini_views_left,
    std::vector<DeskMiniView*> mini_views_right,
    int shift_x);

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

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_MINI_VIEW_ANIMATIONS_H_
