// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Returns true if |window| can cover available workspace.
bool CanCoverAvailableWorkspace(aura::Window* window);

bool IsOverviewSwipeToCloseEnabled();

// Fades |widget| to opacity one with the enter overview settings. Additionally
// place |widget| closer to the top of screen and slide it down if |slide| is
// true.
void FadeInWidgetAndMaybeSlideOnEnter(views::Widget* widget,
                                      OverviewAnimationType animation_type,
                                      bool slide);

// Fades |widget| to opacity zero with animation settings depending on
// |animation_type|. Used by several classes which need to be destroyed on
// exiting overview, but have some widgets which need to continue animating.
// |widget| is destroyed after finishing animation. Additionally slide |widget|
// towards the top of screen if |slide| is true.
void FadeOutWidgetAndMaybeSlideOnExit(std::unique_ptr<views::Widget> widget,
                                      OverviewAnimationType animation_type,
                                      bool slide);

// Creates and returns a background translucent widget parented in
// |root_window|'s default container and having |background_color|.
// When |border_thickness| is non-zero, a border is created having
// |border_color|, otherwise |border_color| parameter is ignored.
// The new background widget starts with |initial_opacity| and then fades in.
// If |parent| is prvoided the return widget will be parented to that window,
// otherwise its parent will be in kShellWindowId_WallpaperContainer of
// |root_window|.
std::unique_ptr<views::Widget> CreateBackgroundWidget(aura::Window* root_window,
                                                      ui::LayerType layer_type,
                                                      SkColor background_color,
                                                      int border_thickness,
                                                      int border_radius,
                                                      SkColor border_color,
                                                      float initial_opacity,
                                                      aura::Window* parent,
                                                      bool stack_on_top);

// Calculates the bounds of the |transformed_window|. Those bounds are a union
// of all regular (normal and panel) windows in the |transformed_window|'s
// transient hierarchy. The returned Rect is in virtual screen coordinates. The
// returned bounds are adjusted to allow the original |transformed_window|'s
// header to be hidden if |top_inset| is not zero.
gfx::Rect GetTransformedBounds(aura::Window* transformed_window, int top_inset);

// Returns the original target bounds of |window|. The bounds are a union of all
// regular (normal and panel) windows in the window's transient hierarchy.
gfx::Rect GetTargetBoundsInScreen(aura::Window* window);

// Applies the |transform| to |window| and all of its transient children. Note
// |transform| is the transform that is applied to |window| and needs to be
// adjusted for the transient child windows.
void SetTransform(aura::Window* window, const gfx::Transform& transform);

// Checks if we are currently in sliding up on the shelf to hide overview mode.
bool IsSlidingOutOverviewFromShelf();

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
