// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
#define CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_

#include <memory>

#include "base/macros.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_state.h"
#include "cc/layers/layer_impl.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;

// Manages the position of the browser controls.
class CC_EXPORT BrowserControlsOffsetManager {
 public:
  enum AnimationDirection { NO_ANIMATION, SHOWING_CONTROLS, HIDING_CONTROLS };

  static std::unique_ptr<BrowserControlsOffsetManager> Create(
      BrowserControlsOffsetManagerClient* client,
      float controls_show_threshold,
      float controls_hide_threshold);
  virtual ~BrowserControlsOffsetManager();

  // The offset from the window top to the top edge of the controls. Runs from 0
  // (controls fully shown) to negative values (down is positive).
  float ControlsTopOffset() const;
  // The amount of offset of the web content area. Same as the current shown
  // height of the browser controls.
  float ContentTopOffset() const;
  float TopControlsShownRatio() const;
  float TopControlsHeight() const;

  // The amount of offset of the web content area, calculating from the bottom.
  // Same as the current shown height of the bottom controls.
  float ContentBottomOffset() const;
  // Similar to TopControlsHeight(), this method should return a static value.
  // The current animated height should be acquired from ContentBottomOffset().
  float BottomControlsHeight() const;
  float BottomControlsShownRatio() const;

  bool has_animation() const { return animation_direction_ != NO_ANIMATION; }
  AnimationDirection animation_direction() { return animation_direction_; }

  void UpdateBrowserControlsState(BrowserControlsState constraints,
                                  BrowserControlsState current,
                                  bool animate);

  BrowserControlsState PullConstraintForMainThread(
      bool* out_changed_since_commit);

  void ScrollBegin();
  gfx::Vector2dF ScrollBy(const gfx::Vector2dF& pending_delta);
  void ScrollEnd();

  // The caller should ensure that |Pinch{Begin,End}| are called within
  // the scope of |Scroll{Begin,End}|.
  void PinchBegin();
  void PinchEnd();

  void MainThreadHasStoppedFlinging();

  gfx::Vector2dF Animate(base::TimeTicks monotonic_time);

 protected:
  BrowserControlsOffsetManager(BrowserControlsOffsetManagerClient* client,
                               float controls_show_threshold,
                               float controls_hide_threshold);

 private:
  void ResetAnimations();
  void SetupAnimation(AnimationDirection direction);
  void StartAnimationIfNecessary();
  bool IsAnimationComplete(float new_ratio);
  void ResetBaseline();

  // The client manages the lifecycle of this.
  BrowserControlsOffsetManagerClient* client_;

  // animation_initialized_ tracks if we've initialized the start and end
  // times since that must happen at a BeginFrame.
  bool animation_initialized_;
  base::TimeTicks animation_start_time_;
  float animation_start_value_;
  base::TimeTicks animation_stop_time_;
  float animation_stop_value_;
  AnimationDirection animation_direction_;

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

  DISALLOW_COPY_AND_ASSIGN(BrowserControlsOffsetManager);
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_OFFSET_MANAGER_H_
