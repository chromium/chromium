// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_SNAP_ANIMATION_CONTROLLER_H_
#define CC_INPUT_BROWSER_CONTROLS_SNAP_ANIMATION_CONTROLLER_H_

#include <cstdint>
#include <optional>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "cc/cc_export.h"
#include "cc/input/browser_controls_snap_hint.h"
#include "cc/input/scroll_velocity_tracker.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

class BrowserControlsOffsetManagerClient;

// Decides when the browser controls should snap to fully shown or fully hidden
// during and after a scroll gesture.
//
// This class is a pure decision-maker over scroll input and controls geometry.
// It knows nothing about how (or whether) its hints are realized, so that the
// animation can eventually run outside the renderer without changing this
// controller.
//
// The snap animation logic divides the page into four vertical regions:
//
//                  +-----------------------------+ Page Top
//                  |                             |
//                  |     Always-Shown Region     | Show controls immediately
//                  |       (from page top)       | when scrolling up.
//                  |                             |
//                  +^^^^^^^^^^^^^^^^^^^^^^^^^^^^^+
//                  |                             |
//                  |          (top gap)          | Prevents show/hide loops.
//                  |                             |
//                  +vvvvvvvvvvvvvvvvvvvvvvvvvvvvv+
//                  |                             |
//                  |       Can-Hide Region       | Allows hiding controls
//                  |                             | when scrolling down.
//                  |                             |
//                  +^^^^^^^^^^^^^^^^^^^^^^^^^^^^^+
//                  |                             |
//                  |         (bottom gap)        | Prevents show/hide loops.
//                  |                             |
//                  +-----------------------------+ Page Bottom
//
// The core goal is to prevent web contents from shifting. Controls should
// neither animate in at the page top nor hide unless there is enough web
// content below to absorb the counter-scroll.
//
// 1. Always-Shown Region: At the very top, the page does not counter-scroll,
//    so an animation would shift the web contents down. By triggering the
//    show animation even if the user has not scrolled up enough in the
//    current scroll sequence, the chance of the controls animating in when
//    scrolled to the top is minimized.
// 2. Top Gap: When top controls hide, the page counter-scrolls upward by the
//    top controls' height. The gap ensures this counter-scroll doesn't push
//    the viewport top back into the Always-Shown region, which can
//    instantly trigger a show at the end of the hide animation.
// 3. Can-Hide Region: Starts dynamically below the gap. Slower scrolls
//    push the start further down to prevent the user from scrolling up
//    while the hide animation is running and reaching the page top before a
//    show animation can start.
// 4. Bottom Gap: When bottom controls hide, the page
//    counter-scrolls upward by the bottom controls' height. The gap ensures
//    this counter-scroll doesn't push the viewport top back into the
//    Always-Shown region, which can instantly trigger a show at the end of
//    the hide animation.
//
// Note that if the page is short enough, the top and bottom gaps will
// overlap, and the Can-Hide Region will not exist.
class CC_EXPORT BrowserControlsSnapAnimationController {
 public:
  BrowserControlsSnapAnimationController(
      BrowserControlsOffsetManagerClient* client,
      int64_t show_hide_max_duration_ms);
  BrowserControlsSnapAnimationController(
      const BrowserControlsSnapAnimationController&) = delete;
  BrowserControlsSnapAnimationController& operator=(
      const BrowserControlsSnapAnimationController&) = delete;
  ~BrowserControlsSnapAnimationController();

  void OnScrollBegin();
  std::optional<BrowserControlsSnapHint> OnScrollUpdate(
      const gfx::Vector2dF& scroll_delta,
      bool is_inertial);
  std::optional<BrowserControlsSnapHint> OnScrollEnd();

  // Returns the slowness factor (between 0 and 1) based on how long it would
  // take to animate the controls to their final position given the current
  // scroll velocity compared to the maximum animation duration.
  float Slowness() const;

  // Returns the height of the browser controls that is animated (i.e. total
  // height minus minimum height), prioritizing top controls over bottom
  // controls.
  float ControlsAnimatedHeight() const;

  // Returns true if the viewport is (or will be, after applying
  // `pending_scroll_delta_y`) within the always-shown region near the top of
  // the page.
  bool ViewportInAlwaysShownRegion(float pending_scroll_delta_y = 0.0f) const;

  // Returns the viewport Y-offset below which (i.e. near the top of the page)
  // the browser controls must always be shown.
  float AlwaysShownRegionHeight() const;

  // Returns a pair of floats representing the can-hide region heights from the
  // top and bottom of the page respectively.
  //
  // `slowness` is a multiplier that controls how much the height of the
  // can-hide region changes relative to the height of the controls, where a
  // higher value means a larger region and a lower value means a smaller
  // region. The value should be between 0 and 1.
  std::pair<float, float> CanHideRegionHeights(float slowness) const;

  // Returns the magnitude of scroll delta in a single scroll sequence required
  // to trigger the snap animation.
  //
  // `slowness` is a multiplier that controls how much the trigger threshold
  // changes relative to the reference threshold, where a higher value means
  // a larger threshold and a lower value means a smaller threshold. The value
  // should be between 0 and 1.
  float TriggerThreshold(float slowness) const;

 private:
  // Returns true if the viewport scroll offset is within the can-hide region
  // (i.e. past the top gap and before the bottom gap) for the given
  // `slowness`.
  bool ViewportInCanHideRegion(float slowness) const;

  // Computes the snap hint (if any) based on the current scroll velocity,
  // accumulated scroll delta, and viewport position.
  //
  // `pending_scroll_delta_y` is the current scroll update delta (or 0 on
  // scroll end), used to check whether the viewport is expected to enter the
  // always-shown region.
  // `is_scroll_end_or_inertial` indicates whether the user has lifted their
  // finger (scroll end or inertial scroll), which uses the minimum can-hide
  // region height when deciding whether to hide the controls.
  std::optional<BrowserControlsSnapHint> ComputeHint(
      float pending_scroll_delta_y,
      bool is_scroll_end_or_inertial) const;
  std::optional<BrowserControlsSnapHint::Action> ComputeHintAction(
      float pending_scroll_delta_y,
      float slowness,
      bool use_minimum_can_hide_region) const;

  float TopControlsAnimatedHeight() const;
  float BottomControlsAnimatedHeight() const;

  raw_ptr<BrowserControlsOffsetManagerClient> client_;
  const int64_t show_hide_max_duration_ms_;
  ScrollVelocityTracker scroll_velocity_tracker_;
  float accumulated_scroll_delta_y_ = 0.0f;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_SNAP_ANIMATION_CONTROLLER_H_
