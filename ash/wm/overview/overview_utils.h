// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/vector_icon_types.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

namespace ash {
class OverviewItemBase;

// Returns true if an overview session is active.
ASH_EXPORT bool IsInOverviewSession();

// Returns the overview session if overview mode is active, otherwise returns
// nullptr.
ASH_EXPORT OverviewSession* GetOverviewSession();

// Returns true if `window` can cover available workspace.
bool CanCoverAvailableWorkspace(aura::Window* window);

// Fades `widget` to opacity one and sets the transform to target with the enter
// overview settings. Have OverviewController observe this animation as a enter
// animation if `observe` is true.
void FadeInAndTransformWidgetToOverview(views::Widget* widget,
                                        const gfx::Transform& target_transform,
                                        OverviewAnimationType animation_type,
                                        bool observe);

// Fades `widget` to opacity one with the enter overview settings.
// Have OverviewController observe this animation as a enter animation if
// `observe` is true.
ASH_EXPORT void FadeInWidgetToOverview(views::Widget* widget,
                                       OverviewAnimationType animation_type,
                                       bool observe);

// Makes `widget` not be able to process events. This should only be used if
// `widget` is shutting down with animation, as `widget` will no longer be
// interactable during the process.
void PrepareWidgetForShutdownAnimation(views::Widget* widget);

// Fades `widget` to opacity zero with animation settings depending on
// `animation_type`. Used by several classes which need to be destroyed on
// exiting overview, but have some widgets which need to continue animating.
// `widget` is destroyed after finishing animation.
void FadeOutWidgetFromOverview(std::unique_ptr<views::Widget> widget,
                               OverviewAnimationType animation_type);

// Takes ownership of `widget`, closes and destroys it without any animations.
void ImmediatelyCloseWidgetOnExit(std::unique_ptr<views::Widget> widget);

// Returns the original bounds for the given `window` outside of overview. The
// bounds are a union of all regular (normal and transient) windows in the
// window's transient hierarchy.
ASH_EXPORT gfx::RectF GetUnionScreenBoundsForWindow(aura::Window* window);

// Returns the corresponding `OverviewItemFillMode` with given `size`.
OverviewItemFillMode GetOverviewItemFillMode(const gfx::Size& size);

// Returns the corresponding `OverviewItemFillMode` for the given `window`:
//  - For independent `OverviewItem`s, any `OverviewItemFillMode` are allowed.
//  - For `OverviewItem`s within an `OverviewGroupItem`, only the default
//  `kNormal` mode is allowed. (This restriction is in place to avoid visual
//  glitches and header misalignment problems on the header view).
OverviewItemFillMode GetOverviewItemFillModeForWindow(aura::Window* window);

// Maximize the window if it is snapped without animation.
void MaximizeIfSnapped(aura::Window* window);

// Get the grid bounds if a window is snapped in splitview, or what they will be
// when snapped based on `target_root` and `indicator_state`. If
// `account_for_hotseat` is true and we are in tablet mode, inset the bounds by
// the hotseat size.
ASH_EXPORT gfx::Rect GetGridBoundsInScreen(aura::Window* target_root);
gfx::Rect GetGridBoundsInScreen(
    aura::Window* target_root,
    std::optional<SplitViewDragIndicators::WindowDraggingState>
        window_dragging_state,
    bool account_for_hotseat);

// Gets the bounds of a window if it were to be snapped or about to be snapped
// in splitview. Returns nothing if we are not in tablet mode, or if we aren't
// in splitview, or if we aren't showing a splitview preview.
std::optional<gfx::RectF> GetSplitviewBoundsMaintainingAspectRatio();

// Check if the grid layout in tablet mode should be used.
bool ShouldUseTabletModeGridLayout();

// Returns a Rect by rounding the values of the given RectF in a way that
// returns the same size for SizeF regardless of its origin.
ASH_EXPORT gfx::Rect ToStableSizeRoundedRect(const gfx::RectF& rect);

// Determines if an `item` is eligible for snapping in Overview. Snapping is
// disallowed for `OverviewGroupItem`s holding two `OverviewItem`s.
bool IsEligibleForDraggingToSnapInOverview(OverviewItemBase* item);

// For all `windows`, change their visibility by changing the window opacity,
// animating where necessary.
void SetWindowsVisibleDuringItemDragging(const aura::Window::Windows& windows,
                                         bool visible,
                                         bool animate);

// Generates and stylizes the icon for menu item.
ui::ImageModel CreateIconForMenuItem(const gfx::VectorIcon& icon);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
