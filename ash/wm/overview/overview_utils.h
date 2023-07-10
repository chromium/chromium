// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Returns true if `window` can cover available workspace.
bool CanCoverAvailableWorkspace(aura::Window* window);

// Returns false if any window with `root_window` covers the entire workspace,
// true otherwise.
bool ShouldAnimateWallpaper(aura::Window* root_window);

// Fades `widget` to opacity one with the enter overview settings.
// Have OverviewController observe this animation as a enter animation if
// `observe` is true.
void FadeInWidgetToOverview(views::Widget* widget,
                            OverviewAnimationType animation_type,
                            bool observe);

// Fades `widget` to opacity zero with animation settings depending on
// `animation_type`. Used by several classes which need to be destroyed on
// exiting overview, but have some widgets which need to continue animating.
// `widget` is destroyed after finishing animation.
void FadeOutWidgetFromOverview(std::unique_ptr<views::Widget> widget,
                               OverviewAnimationType animation_type);

// Takes ownership of `widget`, closes and destroys it without any animations.
void ImmediatelyCloseWidgetOnExit(std::unique_ptr<views::Widget> widget);

// Returns the original target bounds of `window`. The bounds are a union of all
// regular (normal and panel) windows in the window's transient hierarchy.
gfx::RectF GetTargetBoundsInScreen(aura::Window* window);

// Applies the `transform` to `window` and all of its transient children. Note
// `transform` is the transform that is applied to `window` and needs to be
// adjusted for the transient child windows.
ASH_EXPORT void SetTransform(aura::Window* window,
                             const gfx::Transform& transform);

// Maximize the window if it is snapped without animation.
void MaximizeIfSnapped(aura::Window* window);

// Get the grid bounds if a window is snapped in splitview, or what they will be
// when snapped based on `target_root` and `indicator_state`. If
// `divider_changed` is true, maybe clamp the bounds to a minimum size and shift
// the bounds offscreen. If `account_for_hotseat` is true and we are in tablet
// mode, inset the bounds by the hotseat size.
gfx::Rect GetGridBoundsInScreen(aura::Window* target_root);
gfx::Rect GetGridBoundsInScreen(
    aura::Window* target_root,
    absl::optional<SplitViewDragIndicators::WindowDraggingState>
        window_dragging_state,
    bool divider_changed,
    bool account_for_hotseat);

// Gets the bounds of a window if it were to be snapped or about to be snapped
// in splitview. Returns nothing if we are not in tablet mode, or if we aren't
// in splitview, or if we aren't showing a splitview preview.
absl::optional<gfx::RectF> GetSplitviewBoundsMaintainingAspectRatio();

// Check if kNewOverviewLayout is enabled for tablet mode.
bool ShouldUseTabletModeGridLayout();

// Returns a Rect by rounding the values of the given RectF in a way that
// returns the same size for SizeF regardless of its origin.
ASH_EXPORT gfx::Rect ToStableSizeRoundedRect(const gfx::RectF& rect);

void UpdateOverviewHighlightForFocus(OverviewHighlightableView* target_view);
}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
