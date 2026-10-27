// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_VIEW_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_VIEW_H_

#include <cstddef>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/view.h"

namespace views {
class ImageButton;
}  // namespace views

namespace glic {

// Native Views loading skeleton displayed in the Glic side panel or floating
// window while the Glic web client is loading, avoiding the overhead of a
// dedicated WebUI WebContents (`chrome://glic/overlay`) for the common loading
// path.
class GlicLoadingPlaceholderView : public views::View,
                                   public gfx::AnimationDelegate {
  METADATA_HEADER(GlicLoadingPlaceholderView, views::View)

 public:
  enum class Mode {
    kSidePanel,
    kFloating,
  };

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kElementIdForTesting);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonElementIdForTesting);

  GlicLoadingPlaceholderView(Mode mode, base::RepeatingClosure close_callback);
  GlicLoadingPlaceholderView(const GlicLoadingPlaceholderView&) = delete;
  GlicLoadingPlaceholderView& operator=(const GlicLoadingPlaceholderView&) =
      delete;
  ~GlicLoadingPlaceholderView() override;

  void SetBackgroundRoundedCorners(const gfx::RoundedCornersF& radii);

  // Returns true when `point_in_placeholder` (in this view's local coordinates)
  // lies inside the floating header's draggable area, excluding the close
  // button. Always returns false in `Mode::kSidePanel` or when hidden.
  bool IsPointWithinDraggableHeader(
      const gfx::Point& point_in_placeholder) const;

  // views::View:
  void VisibilityChanged(views::View* starting_from, bool is_visible) override;
  void AddedToWidget() override;
  void RemovedFromWidget() override;

  // gfx::AnimationDelegate:
  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationProgressed(const gfx::Animation* animation) override;

  size_t block_count_for_testing() const { return blocks_.size(); }
  views::View* block_for_testing(size_t index) {
    return blocks_.at(index).view;
  }
  Mode mode_for_testing() const { return mode_; }
  views::ImageButton* close_button_for_testing() { return close_button_; }
  bool is_animating_for_testing() const {
    return pulse_animation_.is_animating();
  }
  void ApplyAnimationElapsedForTesting(base::TimeDelta elapsed);

 private:
  struct AnimatedBlock {
    raw_ptr<views::View> view = nullptr;
    base::TimeDelta delay;
  };

  void BuildSidePanelLayout();
  void BuildFloatingLayout();
  std::unique_ptr<views::View> CreatePlaceholderBlock(
      const gfx::Size& preferred_size,
      float corner_radius,
      base::TimeDelta delay);
  std::unique_ptr<views::ImageButton> CreateCloseButton();
  void UpdateBackgroundLayer();
  void UpdateAnimationState();
  void ResetBlockOpacities();
  void UpdateBlockOpacities(double total_elapsed_ms);

  const Mode mode_;
  base::RepeatingClosure close_callback_;
  gfx::RoundedCornersF radii_;

  raw_ptr<views::View> header_view_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  std::vector<AnimatedBlock> blocks_;

  gfx::LinearAnimation pulse_animation_;
  int cycle_count_ = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_LOADING_PLACEHOLDER_VIEW_H_
