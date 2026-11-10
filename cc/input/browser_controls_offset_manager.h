// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/input/browser_controls_animation.h"
#include "cc/input/browser_controls_offset_manager_interface.h"
#include "cc/input/browser_controls_snap_animation_controller.h"
#include "cc/input/browser_controls_snap_hint.h"
#include "cc/input/browser_controls_state.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/browser_controls_params.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;

// Manages the position of the browser controls. Moves the controls in
// response to compositor scrolls and runs the show, hide and height-change
// animations.
class CC_EXPORT BrowserControlsOffsetManager
    : public BrowserControlsOffsetManagerInterface {
 public:
  static std::unique_ptr<BrowserControlsOffsetManager> Create(
      BrowserControlsOffsetManagerClient* client,
      float controls_show_threshold,
      float controls_hide_threshold,
      bool in_viz_process = false);
  BrowserControlsOffsetManager(const BrowserControlsOffsetManager&) = delete;
  ~BrowserControlsOffsetManager() override;

  BrowserControlsOffsetManager& operator=(const BrowserControlsOffsetManager&) =
      delete;

  // BrowserControlsOffsetManagerInterface:
  std::vector<viz::OffsetTagValue> GetOffsetTagValues() const override;
  std::pair<float, float> TopControlsShownRatioRange() override;
  std::pair<float, float> BottomControlsShownRatioRange() override;
  bool HasAnimation() override;
  void OnBrowserControlsParamsChanged(bool animate_changes) override;
  void SetAnimation(const BrowserControlsAnimationParams& params) override;
  void ScrollBegin() override;
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta,
                          bool is_inertial) override;
  void ScrollEnd(const gfx::Vector2dF& compensated_scroll_delta) override;
  void PinchBegin() override;
  void PinchEnd() override;
  gfx::Vector2dF Animate(base::TimeTicks monotonic_time) override;
  void ResetAnimations() override;

  bool IsAnimatingToShowControls() const {
    return (top_controls_animation_.IsInitialized() &&
            top_controls_animation_.direction() ==
                BrowserControlsAnimation::Direction::kShowingControls) ||
           (bottom_controls_animation_.IsInitialized() &&
            bottom_controls_animation_.direction() ==
                BrowserControlsAnimation::Direction::kShowingControls);
  }

  const BrowserControlsSnapAnimationController& snap_controller_for_testing()
      const {
    return snap_controller_.value();
  }

 protected:
  BrowserControlsOffsetManager(BrowserControlsOffsetManagerClient* client,
                               float controls_show_threshold,
                               float controls_hide_threshold,
                               bool in_viz_process = false);

  // BrowserControlsOffsetManagerInterface:
  void OnBrowserControlsStateUpdated(BrowserControlsState constraints,
                                     BrowserControlsState current,
                                     bool animate) override;

 private:
  void SetupAnimation(BrowserControlsAnimation::Direction direction,
                      int64_t duration_ms,
                      gfx::Tween::Type tween_type = gfx::Tween::LINEAR);

  // Starts a show or hide snap animation corresponding to `hint` if no
  // animation is currently running and the controls are not already at the
  // target state. During an active touch scroll
  // (`!hint->is_scroll_end_or_inertial`), a hide hint is ignored if the
  // controls have already hidden once during the current scroll sequence.
  void ApplySnapHint(std::optional<BrowserControlsSnapHint> hint);

  struct ActiveControls {
    float height;
    float shown_ratio;
    float min_shown_ratio;
    float baseline_content_offset;
  };

  // Returns whether scroll and animation calculations should be based on the
  // top controls rather than the bottom controls. Priority order:
  // 1. Scrollable top controls (`TopControlsHeight() > TopControlsMinHeight()`)
  // 2. Scrollable bottom controls
  //    (`BottomControlsHeight() > BottomControlsMinHeight()`)
  // 3. Non-zero top controls (`TopControlsHeight() > 0.f`)
  bool ShouldBaseOnTopControls() const;
  ActiveControls GetActiveControls() const;
  void StartAnimationIfNecessary();
  void ResetBaseline();
  float OldTopControlsMinShownRatio();
  float OldBottomControlsMinShownRatio();
  void UpdateOldBrowserControlsParams();
  void InitAnimationForHeightChange(BrowserControlsAnimation* animation,
                                    float start_ratio,
                                    float stop_ratio);
  void SetTopMinHeightOffsetAnimationRange(float from, float to);
  void SetBottomMinHeightOffsetAnimationRange(float from, float to);
  bool IsAnimatingHeightChange();
  void MaybeRecordHasExistingAnimationHistogram();

  gfx::Vector2dF ScrollByPrecise(const gfx::Vector2dF& pending_delta);

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

  // The old browser controls params that are used to figure out how to animate
  // the height and min-height changes.
  BrowserControlsParams old_browser_controls_params_;

  // Whether a min-height change animation is in progress.
  bool top_min_height_change_in_progress_;
  bool bottom_min_height_change_in_progress_;

  // Minimum and maximum values TopControlsMinHeightOffset() can take
  // during the current min-height change animation.
  std::optional<std::pair<float, float>> top_min_height_offset_animation_range_;
  // Minimum and maximum values BottomControlsMinHeightOffset() can take
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

  // Used to track if the browser controls hide animation ran during the current
  // scroll sequence.
  bool did_hide_this_scroll_ = false;

  // Whether the currently active animation was initiated by a scroll snap
  // animation rather than an explicit browser state update. Only set while
  // |extra_hide_gestures_enabled_| is true.
  bool is_snap_animation_ = false;

  // If set to true, an explicit hide (e.g. from a toolbar swipe) that completes
  // in the always-shown region is not undone by a snap animation to show.
  const bool extra_hide_gestures_enabled_ = false;
  const bool in_viz_process_ = false;

  BrowserControlsAnimation top_controls_animation_;
  BrowserControlsAnimation bottom_controls_animation_;

  std::optional<BrowserControlsSnapAnimationController> snap_controller_;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
