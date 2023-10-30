// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SINGLE_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
#define CC_INPUT_SINGLE_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/scrollbar.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class ScrollbarAnimationControllerClient;

// ScrollbarAnimationControllerThinning for one scrollbar
class CC_EXPORT SingleScrollbarAnimationControllerThinning {
 public:
  static std::unique_ptr<SingleScrollbarAnimationControllerThinning> Create(
      ElementId scroll_element_id,
      ScrollbarOrientation orientation,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta thinning_duration,
      float idle_thickness_scale);

  SingleScrollbarAnimationControllerThinning(
      const SingleScrollbarAnimationControllerThinning&) = delete;
  ~SingleScrollbarAnimationControllerThinning() = default;

  SingleScrollbarAnimationControllerThinning& operator=(
      const SingleScrollbarAnimationControllerThinning&) = delete;

  bool mouse_is_over_scrollbar_thumb() const {
    return mouse_is_over_scrollbar_thumb_;
  }
  bool mouse_is_near_scrollbar_thumb() const {
    return mouse_is_near_scrollbar_thumb_;
  }
  bool mouse_is_near_scrollbar_track() const {
    return mouse_is_near_scrollbar_track_;
  }

  bool captured() const { return captured_; }
  gfx::PointF device_viewport_last_pointer_location() const {
    return device_viewport_last_pointer_location_;
  }

  bool Animate(base::TimeTicks now);
  void StartAnimation();
  void StopAnimation();

  void DidScrollUpdate();
  void DidRequestShow();

  void DidMouseDown();
  void DidMouseUp();
  void DidMouseLeave();
  void DidMouseMove(const gfx::PointF& device_viewport_point);

  float MouseMoveDistanceToTriggerExpand();
  float MouseMoveDistanceToTriggerFadeIn();

  void UpdateTickmarksVisibility(bool show);

 private:
  SingleScrollbarAnimationControllerThinning(
      ElementId scroll_element_id,
      ScrollbarOrientation orientation,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta thinning_duration,
      float idle_thickness_scale);

  ScrollbarLayerImplBase* GetScrollbar() const;
  float AnimationProgressAtTime(base::TimeTicks now);
  void RunAnimationFrame(float progress);

  // Describes whether the current animation should kIncrease (thicken)
  // a bar or kDecrease it (thin).
  enum class AnimationChange { kNone, kIncrease, kDecrease };
  float ThumbThicknessScaleAt(float progress) const;
  float CurrentForcedThumbThicknessScale() const;
  void CalculateThicknessShouldChange(const gfx::PointF& device_viewport_point);

  float AdjustScale(float new_value,
                    float current_value,
                    AnimationChange animation_change,
                    float min_value,
                    float max_value);
  void UpdateThumbThicknessScale();
  void ApplyThumbThicknessScale(float thumb_thickness_scale);

  raw_ptr<ScrollbarAnimationControllerClient> client_;

  base::TimeTicks last_awaken_time_;
  bool is_animating_;

  ElementId scroll_element_id_;

  ScrollbarOrientation orientation_;
  bool captured_;
  bool mouse_is_over_scrollbar_thumb_;
  bool mouse_is_near_scrollbar_thumb_;
  // For Fluent scrollbars the near distance to the track is 0 which is
  // equivalent to the mouse being over the thumb/track.
  bool mouse_is_near_scrollbar_track_;
  // Are we narrowing or thickening the bars.
  AnimationChange thickness_change_;

  base::TimeDelta thinning_duration_;

  bool tickmarks_showing_;
  // Save last known pointer location in the device viewport for use in
  // DidScrollUpdate() to check the pointers proximity to the thumb in case of a
  // scroll.
  gfx::PointF device_viewport_last_pointer_location_{-1, -1};
  const float idle_thickness_scale_;
};

}  // namespace cc

#endif  // CC_INPUT_SINGLE_SCROLLBAR_ANIMATION_CONTROLLER_THINNING_H_
