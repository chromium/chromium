// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_

#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// An ImageView of the ghosting icon to show where a dragged app or folder
// will drop on the app list. This view is owned by the client and not the
// view hierarchy.
class ASH_EXPORT GhostImageView : public views::ImageView,
                                  public ui::ImplicitAnimationObserver {
  METADATA_HEADER(GhostImageView, views::ImageView)

 public:
  explicit GhostImageView(GridIndex index);

  GhostImageView(const GhostImageView&) = delete;
  GhostImageView& operator=(const GhostImageView&) = delete;

  ~GhostImageView() override;

  // Initialize the GhostImageView.
  void Init(const gfx::Rect& drop_target_bounds, int grid_focus_corner_radius);

  // Begins the fade out animation.
  void FadeOut();

  // Begins the fade in animation
  void FadeIn();

  // Set the offset used for page transitions.
  void SetTransitionOffset(const gfx::Vector2d& bounds_rect);

  GridIndex index() const { return index_; }

 private:
  // Start the animation for showing or for hiding the GhostImageView.
  void DoAnimation(bool hide);

  // views::ImageView overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // Whether the view is hiding.
  bool is_hiding_;

  // The corner radius for the painted rect.
  int corner_radius_ = 0;

  // The page and slot for this view in the parent apps grid view.
  const GridIndex index_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
