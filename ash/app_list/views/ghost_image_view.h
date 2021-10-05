// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_

#include <vector>

#include "base/macros.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/image_view.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ash {

class AppListConfig;
class AppListItem;

// An ImageView of the ghosting icon to show where a dragged app or folder
// will drop on the app list. This view is owned by the client and not the
// view hierarchy.
class GhostImageView : public views::ImageView,
                       public ui::ImplicitAnimationObserver {
 public:
  GhostImageView(AppListItem* item, bool is_in_folder);

  GhostImageView(const GhostImageView&) = delete;
  GhostImageView& operator=(const GhostImageView&) = delete;

  ~GhostImageView() override;

  // Initialize the GhostImageView.
  void Init(const AppListConfig* app_list_config,
            const gfx::Rect& icon_bounds,
            const gfx::Rect& drop_target_bounds);

  // Begins the fade out animation.
  void FadeOut();

  // Begins the fade in animation
  void FadeIn();

  // Set the offset used for page transitions.
  void SetTransitionOffset(const gfx::Vector2d& bounds_rect);

  // views::View:
  const char* GetClassName() const override;

 private:
  // Start the animation for showing or for hiding the GhostImageView.
  void DoAnimation(bool hide);

  // views::ImageView overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // Whether the view is hiding.
  bool is_hiding_;

  // Whether the view is in a folder.
  bool is_in_folder_;

  // Whether the view is the ghost of a folder.
  bool is_folder_;

  // The radius used for drawing the icons shown inside the folder ghost image.
  int inner_icon_radius_;

  // Icon bounds used to determine size and placement of the GhostImageView.
  gfx::Rect icon_bounds_;

  // The number of items within the GhostImageView folder.
  const size_t num_items_;

  // The origins of the top icons within a folder icon. Used for the folder
  // ghost image.
  std::vector<gfx::Point> inner_folder_icon_origins_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
