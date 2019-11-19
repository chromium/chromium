// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_

#include "base/macros.h"
#include "base/optional.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class AppListItemView;

// An ImageView of the ghosting icon to show where a dragged app or folder
// will drop on the app list. This view is owned by the client and not the
// view hierarchy.
class GhostImageView : public views::ImageView,
                       public ui::ImplicitAnimationObserver {
 public:
  GhostImageView(bool is_folder, bool is_in_folder, int page);
  ~GhostImageView() override;

  // Initialize the GhostImageView.
  void Init(AppListItemView* drag_view, const gfx::Rect& drop_target_bounds);

  // Begins the fade out animation.
  void FadeOut();

  // Begins the fade in animation
  void FadeIn();

  // Set the offset used for page transitions.
  void SetTransitionOffset(const gfx::Vector2d& bounds_rect);

  // Returns the page number which this view belongs to.
  int page() const { return page_; }

  // views::View:
  const char* GetClassName() const override;

 private:
  // Start the animation for showing or for hiding the GhostImageView.
  void DoAnimation(bool hide);

  // views::ImageView overrides:
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::ImplicitAnimationObserver overrides:
  void OnImplicitAnimationsCompleted() override;

  // Returns an ImageSkia with just an outline of the input ImageSkia.
  gfx::ImageSkia GetIconOutline(const gfx::ImageSkia& original_icon);

  // Whether the view is hiding.
  bool is_hiding_;

  // Whether the view is in a folder.
  bool is_in_folder_;

  // Whether the view is the ghost of a folder.
  bool is_folder_;

  // Page this this view belongs to, used to calculate transition offset.
  int page_;

  // Icon bounds used to determine size and placement of the GhostImageView.
  gfx::Rect icon_bounds_;

  // The number of items within the GhostImageView folder.
  base::Optional<size_t> num_items_;

  // The outline of the dragged item's icon. Used as the ghost image.
  gfx::ImageSkia outline_;

  // The outlines of the top icons within a folder. Used for the folder ghost
  // image.
  std::vector<gfx::ImageSkia> inner_folder_icon_outlines_;

  DISALLOW_COPY_AND_ASSIGN(GhostImageView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_GHOST_IMAGE_VIEW_H_
