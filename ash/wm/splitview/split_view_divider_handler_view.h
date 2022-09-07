// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_HANDLER_VIEW_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_HANDLER_VIEW_H_

#include <memory>

#include "ui/gfx/canvas.h"
#include "ui/views/view.h"

namespace ash {

// The white handler bar in the middle of the divider.
class SplitViewDividerHandlerView : public views::View {
 public:
  SplitViewDividerHandlerView();

  SplitViewDividerHandlerView(const SplitViewDividerHandlerView&) = delete;
  SplitViewDividerHandlerView& operator=(const SplitViewDividerHandlerView&) =
      delete;

  ~SplitViewDividerHandlerView() override;

  // Play the white handler's part in the divider spawn animation.
  // |divider_signed_offset| represents the motion of the center of the divider
  // during the spawning animation. This parameter is used to make the white
  // handler move with the center of the divider, as the two views are siblings
  // because if the white handler view were a child of the divider view, then
  // the transform that enlarges the divider during dragging would distort the
  // white handler.
  void DoSpawningAnimation(int divider_signed_offset);

  // If the spawning animation is running, stop it and show the white handler.
  // Update bounds. Do the enlarge/shrink animation when starting/ending
  // dragging.
  void Refresh(bool is_resizing);

  // Updates the corner radius of the handler bar to |radius|. Happens during
  // the animation of starting and ending dragging.
  void UpdateCornerRadius(float radius);

 private:
  class SelectionAnimation;
  class SpawningAnimation;

  void SetBounds(int short_length, int long_length, int signed_offset);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Handles the animations for starting and ending dragging.
  std::unique_ptr<SelectionAnimation> selection_animation_;

  // Handles the spawning animation.
  std::unique_ptr<SpawningAnimation> spawning_animation_;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_DIVIDER_HANDLER_VIEW_H_
