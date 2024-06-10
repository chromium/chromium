// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_

#include <memory>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "cc/input/browser_controls_state.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/browser_controls_params.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;

// Manages the position of the browser controls.
class CC_EXPORT BrowserControlsOffsetManager {
 public:
  enum class AnimationDirection {
    kNoAnimation,
    kShowingControls,
    kHidingControls
  };

  static std::unique_ptr<BrowserControlsOffsetManager> Create(
      BrowserControlsOffsetManagerClient* client,
      float controls_show_threshold,
      float controls_hide_threshold);
  BrowserControlsOffsetManager(const BrowserControlsOffsetManager&) = delete;
  virtual ~BrowserControlsOffsetManager();

  BrowserControlsOffsetManager& operator=(const BrowserControlsOffsetManager&) =
      delete;

  // The offset from the window top to the top edge of the controls. Runs from 0
  // (controls fully shown) to negative values (down is positive).
  float ControlsTopOffset() const;
  // The amount of offset of the web content area. Same as the current shown
  // height of the browser controls.
  float ContentTopOffset() const;
  float TopControlsShownRatio() const;
  float TopControlsHeight() const;
  float TopControlsMinHeight() const;
  // The minimum shown ratio top controls can have.
  float TopControlsMinShownRatio() const;
  // The current top controls min-height. If the min-height is changing with an
  // animation, this will return a value between the old min-height and the new
  // min-height, which is equal to the current visible min-height. Otherwise,
  // this will return the same value as |TopControlsMinHeight()|.
  float TopControlsMinHeightOffset() const;
  viz::OffsetTag TopControlsOffsetTag() const;

  // The amount of offset of the web content area, calculating from the bottom.
  // Same as the current shown height of the bottom controls.
  float ContentBottomOffset() const;
  // Similar to TopControlsHeight(), this method should return a static value.
  // The current animated height should be acquired from ContentBottomOffset().
  float BottomControlsHeight() const;
  float BottomControlsMinHeight() const;
  float BottomControlsShownRatio() const;
  // The minimum shown ratio bottom controls can have.
  float BottomControlsMinShownRatio() const;
  // The current bottom controls min-height. If the min-height is changing with
  // an animation, this will return a value between the old min-height and the
  // new min-height, which is equal to the current visible min-height.
  // Otherwise, this will return the same value as |BottomControlsMinHeight()|.
  float BottomControlsMinHeightOffset() const;

  // Valid shown ratio range for the top controls. The values will be (0, 1) if
  // there is no animation running.
  std::pair<float, float> TopControlsShownRatioRange();
  // Valid shown ratio range for the bottom controls. The values will be (0, 1)
  // if there is no animation running.
  std::pair<float, float> BottomControlsShownRatioRange();

  bool HasAnimation();
  bool IsAnimatingToShowControls() const {
    return top_controls_animation_.IsInitialized() &&
           top_controls_animation_.Direction() ==
               AnimationDirection::kShowingControls;
  }

  // See UpdateBrowserControlsState in
  // third_party/blink/public/mojom/frame/frame.mojom
  void UpdateBrowserControlsState(
      BrowserControlsState constraints,
      BrowserControlsState current,
      bool animate,
      base::optional_ref<const BrowserControlsOffsetTagsInfo> offset_tags_info);

  // Return the browser control constraint that must be synced to the
  // main renderer thread (to trigger viewport and related changes).
  BrowserControlsState PullConstraintForMainThread(
      bool* out_changed_since_commit);
  // Called to notify this object that the control constraint has
  // been pushed to the main thread. When a compositor commit does not
  // happen the value pulled by the method above may not be synced;
  // a call to this method notifies us that it has.
  void NotifyConstraintSyncedToMainThread();

  void OnBrowserControlsParamsChanged(bool animate_changes);

  void ScrollBegin();
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta);
  void ScrollEnd();

  // The caller should ensure that |Pinch{Begin,End}| are called within
  // the scope of |Scroll{Begin,End}|.
  void PinchBegin();
  void PinchEnd();

  gfx::Vector2dF Animate(base::TimeTicks monotonic_time);

  // Predict what the outer viewport container bounds delta will be as browser
  // controls are shown or hidden during a scroll gesture before the Blink
  // WebView is resized to reflect the new state.
  double PredictViewportBoundsDelta(double current_bounds_delta,
                                    gfx::Vector2dF scroll_distance);

  void ResetAnimations();

 protected:
  BrowserControlsOffsetManager(BrowserControlsOffsetManagerClient* client,
                               float controls_show_threshold,
                               float controls_hide_threshold);

 private:
  class Animation;

  void SetupAnimation(AnimationDirection direction);
  void StartAnimationIfNecessary();
  void ResetBaseline();
  float OldTopControlsMinShownRatio();
  float OldBottomControlsMinShownRatio();
  void UpdateOldBrowserControlsParams();
  void InitAnimationForHeightChange(Animation* animation,
                                    float start_ratio,
                                    float stop_ratio);
  void SetTopMinHeightOffsetAnimationRange(float from, float to);
  void SetBottomMinHeightOffsetAnimationRange(float from, float to);

  // The client manages the lifecycle of this.
  raw_ptr<BrowserControlsOffsetManagerClient> client_;

  BrowserControlsState permitted_state_;

  // Accumulated scroll delta since last baseline reset
  float accumulated_scroll_delta_;

  // Content offset when last baseline reset occurred.
  float baseline_top_content_offset_;
  float baseline_bottom_content_offset_;

  // The percent height of the visible control such that it must be shown
  // when the user stops the scroll.
  float controls_show_threshold_;

  // The percent height of the visible control such that it must be hidden
  // when the user stops the scroll.
  float controls_hide_threshold_;

  bool pinch_gesture_active_;

  // Used to track whether the constraint has changed and we need up reflect
  // the changes to Blink.
  bool constraint_changed_since_commit_;

  // The old browser controls params that are used to figure out how to animate
  // the height and min-height changes.
  BrowserControlsParams old_browser_controls_params_;

  // Whether a min-height change animation is in progress.
  bool top_min_height_change_in_progress_;
  bool bottom_min_height_change_in_progress_;

  // Current top/bottom controls min-height.
  float top_controls_min_height_offset_;
  float bottom_controls_min_height_offset_;

  // Minimum and maximum values |top_controls_min_height_offset_| can take
  // during the current min-height change animation.
  std::optional<std::pair<float, float>> top_min_height_offset_animation_range_;
  // Minimum and maximum values |bottom_controls_min_height_offset_| can take
  // during the current min-height change animation.
  std::optional<std::pair<float, float>>
      bottom_min_height_offset_animation_range_;

  // Should ScrollEnd() animate the controls into view?  This is used if there's
  // a race between chrome starting an animation to show the controls while the
  // user is doing a scroll gesture, which would cancel animations.  We want to
  // err on the side of showing the controls, so that the user realizes that
  // they're an option. If we have started, but not yet completed an animation
  // to show the controls when the scroll starts, or if one starts during the
  // gesture, then we reorder the animation until after the scroll.
  bool show_controls_when_scroll_completes_ = false;

  // The tag used to accompany scroll offsets in the render frame's metadata.
  // During surface aggregation, the layers with the same token will have the
  // corresponding offsets applied.
  viz::OffsetTag top_controls_offset_tag_;

  // Class that holds and manages the state of the controls animations.
  class Animation {
   public:
    Animation();

    // Whether the animation is initialized with a direction and start and stop
    // values.
    bool IsInitialized() const { return initialized_; }
    AnimationDirection Direction() const { return direction_; }
    void Initialize(AnimationDirection direction,
                    float start_value,
                    float stop_value,
                    int64_t duration,
                    bool jump_to_end_on_reset);
    // Returns the animated value for the given monotonic time tick if the
    // animation is initialized. Otherwise, returns |std::nullopt|.
    std::optional<float> Tick(base::TimeTicks monotonic_time);
    // Set the minimum and maximum values the animation can have.
    void SetBounds(float min, float max);
    // Reset the properties. If |skip_to_end_on_reset_| is false, this function
    // will return |std::nullopt|. Otherwise, it will return the end value
    // (clamped to min-max).
    std::optional<float> Reset();

    // Returns the value the animation will end on. This will be the stop_value
    // passed to the constructor clamped by the currently configured bounds.
    float FinalValue();

    // Return the bounds.
    float min_value() { return min_value_; }
    float max_value() { return max_value_; }

   private:
    bool IsComplete(float value);

    // Whether the animation is running.
    bool started_ = false;
    // Whether the animation is initialized by setting start and stop time and
    // values.
    bool initialized_ = false;
    AnimationDirection direction_ = AnimationDirection::kNoAnimation;
    // Monotonic start and stop times.
    base::TimeTicks start_time_;
    base::TimeTicks stop_time_;
    // Animation duration.
    base::TimeDelta duration_;
    // Start and stop values.
    float start_value_ = 0.f;
    float stop_value_ = 0.f;
    // Minimum and maximum values the animation can have, used to decide if the
    // animation is complete.
    float min_value_ = 0.f;
    float max_value_ = 1.f;
    // Whether to fast-forward to end when reset. It is still BCOM's
    // responsibility to actually set the shown ratios using the value returned
    // by ::Reset().
    bool jump_to_end_on_reset_ = false;
  };

  Animation top_controls_animation_;
  Animation bottom_controls_animation_;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
