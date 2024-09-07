// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
#define CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_

#include <memory>

#include "base/cancelable_callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "cc/cc_export.h"
#include "cc/input/single_scrollbar_animation_controller_thinning.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/scrollbar_layer_impl_base.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class CC_EXPORT ScrollbarAnimationControllerClient {
 public:
  virtual void PostDelayedScrollbarAnimationTask(base::OnceClosure task,
                                                 base::TimeDelta delay) = 0;
  virtual void SetNeedsRedrawForScrollbarAnimation() = 0;
  virtual void SetNeedsAnimateForScrollbarAnimation() = 0;
  virtual void DidChangeScrollbarVisibility() = 0;
  virtual ScrollbarSet ScrollbarsFor(ElementId scroll_element_id) const = 0;
  virtual bool IsFluentOverlayScrollbar() const = 0;

 protected:
  virtual ~ScrollbarAnimationControllerClient() {}
};

// This class show scrollbars when scroll and fade out after an idle delay.
// The fade animations works on both scrollbars and is controlled in this class
// This class also passes the mouse state to each
// SingleScrollbarAnimationControllerThinning. The thinning animations are
// independent between vertical/horizontal and are managed by the
// SingleScrollbarAnimationControllerThinnings.
class CC_EXPORT ScrollbarAnimationController {
 public:
  // ScrollbarAnimationController for Android. It only has show & fade out
  // animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAndroid(
      ElementId scroll_element_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta fade_delay,
      base::TimeDelta fade_duration,
      float initial_opacity);

  // ScrollbarAnimationController for Desktop Overlay Scrollbar. It has show &
  // fade out animation and thinning animation.
  static std::unique_ptr<ScrollbarAnimationController>
  CreateScrollbarAnimationControllerAuraOverlay(
      ElementId scroll_element_id,
      ScrollbarAnimationControllerClient* client,
      base::TimeDelta fade_delay,
      base::TimeDelta fade_duration,
      base::TimeDelta thinning_duration,
      float initial_opacity,
      float idle_thickness_scale);

  ~ScrollbarAnimationController();

  bool ScrollbarsHidden() const;
  bool visibility_changed() const { return visibility_changed_; }
  void ClearVisibilityChanged() { visibility_changed_ = false; }

  bool Animate(base::TimeTicks now);

  // WillUpdateScroll expects to be called even if the scroll position won't
  // change as a result of the scroll. Only effect Aura Overlay Scrollbar.
  void WillUpdateScroll();

  // DidScrollUpdate expects to be called only if the scroll position change.
  // Effect both Android and Aura Overlay Scrollbar.
  void DidScrollUpdate();

  void DidMouseDown();
  void DidMouseUp();
  void DidMouseLeave();
  void DidMouseMove(const gfx::PointF& device_viewport_point);

  // Called when we want to show the scrollbars.
  void DidRequestShow();

  void UpdateTickmarksVisibility(bool show);

  // These methods are public for testing.
  bool MouseIsOverScrollbarThumb(ScrollbarOrientation orientation) const;
  bool MouseIsNearScrollbarThumb(ScrollbarOrientation orientation) const;
  bool MouseIsNearScrollbar(ScrollbarOrientation orientation) const;
  bool MouseIsNearAnyScrollbar() const;

  ScrollbarSet Scrollbars() const;

  SingleScrollbarAnimationControllerThinning& GetScrollbarAnimationController(
      ScrollbarOrientation) const;

 private:
  // Describes whether the current animation should FadeIn or FadeOut.
  enum class AnimationChange { kNone, kFadeIn, kFadeOut };

  ScrollbarAnimationController(ElementId scroll_element_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta fade_delay,
                               base::TimeDelta fade_duration,
                               float initial_opacity);

  ScrollbarAnimationController(ElementId scroll_element_id,
                               ScrollbarAnimationControllerClient* client,
                               base::TimeDelta fade_delay,
                               base::TimeDelta fade_duration,
                               base::TimeDelta thinning_duration,
                               float initial_opacity,
                               float idle_thickness_scale);

  // Any scrollbar state update would show scrollbar hen post the delay fade out
  // if needed.
  void UpdateScrollbarState();

  // Returns how far through the animation we are as a progress value from
  // 0 to 1.
  float AnimationProgressAtTime(base::TimeTicks now);
  void RunAnimationFrame(float progress);

  void StartAnimation();
  void StopAnimation();

  void Show();

  void PostDelayedAnimation(AnimationChange animation_change);

  bool Captured() const;

  void ApplyOpacityToScrollbars(float opacity);

  raw_ptr<ScrollbarAnimationControllerClient> client_;

  base::TimeTicks last_awaken_time_;

  base::TimeDelta fade_delay_;

  base::TimeDelta fade_duration_;

  bool need_trigger_scrollbar_fade_in_;

  bool is_animating_;
  AnimationChange animation_change_;

  const ElementId scroll_element_id_;

  base::CancelableOnceClosure delayed_scrollbar_animation_;

  float opacity_;

  const bool show_scrollbars_on_scroll_gesture_;
  const bool need_thinning_animation_;

  bool is_mouse_down_;

  bool tickmarks_showing_;

  bool visibility_changed_ = false;

  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      vertical_controller_;
  std::unique_ptr<SingleScrollbarAnimationControllerThinning>
      horizontal_controller_;

  base::WeakPtrFactory<ScrollbarAnimationController> weak_factory_{this};
};

}  // namespace cc

#endif  // CC_INPUT_SCROLLBAR_ANIMATION_CONTROLLER_H_
