// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_APPS_GRID_ROW_CHANGE_ANIMATOR_H_
#define ASH_APP_LIST_APPS_GRID_ROW_CHANGE_ANIMATOR_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class AnimationSequenceBlock;
class View;
}  // namespace views

namespace ash {

class AppsGridView;
class AppListItemView;

// Manages row change animations for app grid items. Uses a map to keep track of
// all item layer copies for the currently running row change animations.
class AppsGridRowChangeAnimator {
 public:
  explicit AppsGridRowChangeAnimator(AppsGridView* apps_grid_view);
  AppsGridRowChangeAnimator(const AppsGridRowChangeAnimator&) = delete;
  AppsGridRowChangeAnimator& operator=(const AppsGridRowChangeAnimator&) =
      delete;
  ~AppsGridRowChangeAnimator();

  // Invoked when the given `view`'s current bounds and target bounds are in
  // different rows. To avoid moving diagonally, `view` is put into an adjacent
  // position to `target` outside of the grid and fades in while moving to
  // `target`. Meanwhile, a layer copy of `view` would start at `current` and
  // fade out while moving off screen to a position adjacent to `current`. When
  // called on a `view` which is already animating between rows, the location of
  // the layer copy and view is used to calculate new animation parameters.
  void AnimateBetweenRows(AppListItemView* view,
                          const gfx::Rect& current,
                          const gfx::Rect& target,
                          views::AnimationSequenceBlock* animation_sequence);

  // Removes `view` from `row_change_layers_` if it exists in the map.
  void CancelAnimation(views::View* view);

  // Returns whether `row_change_layers_` has layers or if an animation is being
  // set up.
  bool IsAnimating() const;

  int GetNumberOfRowChangeLayersForTest() const {
    return row_change_layers_.size();
  }

 private:
  raw_ptr<const AppsGridView> apps_grid_view_ = nullptr;

  // Whether a new row change animation is currently being set up.
  bool setting_up_animation_ = false;

  // Maps each AppListItemView which is currently animating between rows to its
  // layer copy. The layer copy animates out of the old row while the
  // AppListItemView animates into the new row. Cleared when all bounds
  // animations are completed.
  std::map<views::View*, std::unique_ptr<ui::Layer>> row_change_layers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_APPS_GRID_ROW_CHANGE_ANIMATOR_H_
