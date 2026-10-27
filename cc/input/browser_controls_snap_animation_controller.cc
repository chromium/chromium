// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_snap_animation_controller.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/point_f.h"

namespace cc {
namespace {

constexpr float kAlwaysShownRegionMultiplier = 0.5f;
constexpr float kCanHideRegionMinMultiplier = 1.5f;
constexpr float kCanHideRegionMaxMultiplier = 3.0f;
static_assert(
    kCanHideRegionMinMultiplier >= kAlwaysShownRegionMultiplier + 1.0f,
    "Can hide region must start after always shown region and account for "
    "counter-scrolling due to the controls hiding.");
static_assert(kAlwaysShownRegionMultiplier > 0.0f,
              "Always shown region must be non-zero to minimize the chance of "
              "shifting the web contents when controls are shown.");
constexpr float kTriggerThresholdMinMultiplier = 0.2f;
constexpr float kTriggerThresholdReferenceDp = 30.0f;

}  // namespace

BrowserControlsSnapAnimationController::BrowserControlsSnapAnimationController(
    BrowserControlsOffsetManagerClient* client,
    int64_t show_hide_max_duration_ms)
    : client_(client),
      show_hide_max_duration_ms_(show_hide_max_duration_ms),
      scroll_velocity_tracker_(base::Milliseconds(show_hide_max_duration_ms)) {
  CHECK(client_);
  CHECK_GT(show_hide_max_duration_ms_, 0);
}

BrowserControlsSnapAnimationController::
    ~BrowserControlsSnapAnimationController() = default;

void BrowserControlsSnapAnimationController::OnScrollBegin() {
  accumulated_scroll_delta_y_ = 0.0f;
  scroll_velocity_tracker_.Reset();
}

std::optional<BrowserControlsSnapHint>
BrowserControlsSnapAnimationController::OnScrollUpdate(
    const gfx::Vector2dF& scroll_delta,
    bool is_inertial) {
  accumulated_scroll_delta_y_ += scroll_delta.y();

  float velocity_y = scroll_velocity_tracker_.CurrentVelocity().y();
  float current_y = scroll_delta.y();

  // Clear all scroll deltas if current scroll delta is not in the same
  // direction as the current velocity.
  if (!(velocity_y >= 0 && current_y >= 0) &&
      !(velocity_y <= 0 && current_y <= 0)) {
    scroll_velocity_tracker_.Reset();
  }

  scroll_velocity_tracker_.AddSample(base::TimeTicks::Now(), scroll_delta);

  // Wait for at least two samples to setup the snap animation as the velocity
  // is not as accurate with a single sample.
  if (scroll_velocity_tracker_.num_samples() < 2) {
    return std::nullopt;
  }

  // Use inertial scrolling as a proxy for if the user has lifted their finger
  // off the screen and to allow the minimum can-hide region to be used.
  return ComputeHint(scroll_delta.y(),
                     /*is_scroll_end_or_inertial=*/is_inertial);
}

std::optional<BrowserControlsSnapHint>
BrowserControlsSnapAnimationController::OnScrollEnd() {
  // When a scroll end event is received, it means that the user has lifted
  // their finger off the screen. At this point, the browser controls can be
  // animated with very little risk of the user scrolling in the opposite
  // direction before the animation completes. Thus, use minimum can-hide region
  // and trigger the animation based on the current velocity.
  std::optional<BrowserControlsSnapHint> hint =
      ComputeHint(/*pending_scroll_delta_y=*/0.0f,
                  /*is_scroll_end_or_inertial=*/true);
  scroll_velocity_tracker_.Reset();
  return hint;
}

float BrowserControlsSnapAnimationController::Slowness() const {
  float scroll_speed = std::abs(scroll_velocity_tracker_.CurrentVelocity().y());
  if (scroll_speed == 0.0f) {
    return 1.0f;
  }
  return std::clamp(
      (ControlsAnimatedHeight() / scroll_speed) / show_hide_max_duration_ms_,
      0.0f, 1.0f);
}

float BrowserControlsSnapAnimationController::ControlsAnimatedHeight() const {
  // The top controls have priority because they need to visually be in sync
  // with the web contents.
  float animated_height = client_->TopControlsHeight() > 0
                              ? TopControlsAnimatedHeight()
                              : BottomControlsAnimatedHeight();
  DCHECK_GE(animated_height, 0.0f);
  return animated_height;
}

bool BrowserControlsSnapAnimationController::ViewportInAlwaysShownRegion(
    float pending_scroll_delta_y) const {
  const float always_shown_height = AlwaysShownRegionHeight();
  return always_shown_height > 0.0f &&
         client_->ViewportScrollOffset().y() + pending_scroll_delta_y <=
             always_shown_height;
}

float BrowserControlsSnapAnimationController::AlwaysShownRegionHeight() const {
  // When controls are at the top, trigger the show animation early to prevent
  // the browser controls from being shown when the page has been scrolled all
  // the way to the top and the controls are not obscuring any web content. This
  // is not a concern when the controls are only on the bottom.
  return TopControlsAnimatedHeight() * kAlwaysShownRegionMultiplier;
}

std::pair<float, float>
BrowserControlsSnapAnimationController::CanHideRegionHeights(
    float slowness) const {
  DCHECK_GE(slowness, 0.0f);
  DCHECK_LE(slowness, 1.0f);

  // Hide the browser controls well-past the always shown region (if it exists).
  // The purpose of the can-hide region is to prevent the browser controls from
  // animating to show when the page has been scrolled all the way to the top.
  // It works by delaying the hide animation until the user has scrolled a
  // sufficient distance from the top of the page so that a subsequent show
  // animation does not start when the page is at the top, resulting in the web
  // content being shifted down as part of the animation. The faster the user
  // scrolls, the shorter the duration of the hide animation, and therefore the
  // smaller this region needs to be.
  const float multiplier = gfx::Tween::FloatValueBetween(
      slowness, kCanHideRegionMinMultiplier, kCanHideRegionMaxMultiplier);
  return std::make_pair(TopControlsAnimatedHeight() * multiplier,
                        BottomControlsAnimatedHeight() * multiplier);
}

float BrowserControlsSnapAnimationController::TriggerThreshold(
    float slowness) const {
  DCHECK_GE(slowness, 0.0f);
  DCHECK_LE(slowness, 1.0f);

  // Using a fixed reference threshold makes the experience consistent under
  // browser states that can greatly increase the height of the controls (e.g.
  // tab groups).
  const float reference_threshold =
      client_->DeviceScaleFactor() * kTriggerThresholdReferenceDp;

  // Scale the threshold based on the animation duration. Start the animation
  // early when the animation duration is short, and late when the animation
  // duration is long. This gives the appearance of a consistent, responsive
  // threshold.
  return reference_threshold *
         gfx::Tween::FloatValueBetween(slowness, kTriggerThresholdMinMultiplier,
                                       1.0f);
}

bool BrowserControlsSnapAnimationController::ViewportInCanHideRegion(
    float slowness) const {
  float viewport_offset_y = client_->ViewportScrollOffset().y();
  auto [can_hide_region_height_top, can_hide_region_height_bottom] =
      CanHideRegionHeights(slowness);
  return viewport_offset_y > can_hide_region_height_top &&
         viewport_offset_y < client_->MaxViewportScrollOffsetY() -
                                 can_hide_region_height_bottom;
}

std::optional<BrowserControlsSnapHint>
BrowserControlsSnapAnimationController::ComputeHint(
    float pending_scroll_delta_y,
    bool is_scroll_end_or_inertial) const {
  const float controls_animated_height = ControlsAnimatedHeight();

  // Return early if the controls have no height to animate.
  if (controls_animated_height == 0.0f) {
    return std::nullopt;
  }
  DCHECK_GT(controls_animated_height, 0.0f);

  const float slowness = Slowness();
  std::optional<BrowserControlsSnapHint::Action> action = ComputeHintAction(
      pending_scroll_delta_y, slowness,
      /*use_minimum_can_hide_region=*/is_scroll_end_or_inertial);
  if (!action) {
    return std::nullopt;
  }

  return BrowserControlsSnapHint{*action, slowness, is_scroll_end_or_inertial};
}

std::optional<BrowserControlsSnapHint::Action>
BrowserControlsSnapAnimationController::ComputeHintAction(
    float pending_scroll_delta_y,
    float slowness,
    bool use_minimum_can_hide_region) const {
  const float trigger_threshold = TriggerThreshold(slowness);

  if (scroll_velocity_tracker_.CurrentVelocity().y() >= 0.0f) {
    if (ViewportInCanHideRegion(use_minimum_can_hide_region ? 0.0f
                                                            : slowness) &&
        accumulated_scroll_delta_y_ >= trigger_threshold) {
      return BrowserControlsSnapHint::Action::kCanHide;
    }
  } else {
    if (ViewportInAlwaysShownRegion(pending_scroll_delta_y)) {
      return BrowserControlsSnapHint::Action::kMustShow;
    }

    if (accumulated_scroll_delta_y_ <= -trigger_threshold) {
      return BrowserControlsSnapHint::Action::kCanShow;
    }
  }

  return std::nullopt;
}

float BrowserControlsSnapAnimationController::TopControlsAnimatedHeight()
    const {
  // The min height can transiently exceed the height (e.g. while the browser
  // controls are being resized), so clamp to avoid a negative height.
  return std::max(
      0.0f, client_->TopControlsHeight() - client_->TopControlsMinHeight());
}

float BrowserControlsSnapAnimationController::BottomControlsAnimatedHeight()
    const {
  // The min height can transiently exceed the height (e.g. while the browser
  // controls are being resized), so clamp to avoid a negative height.
  return std::max(0.0f, client_->BottomControlsHeight() -
                            client_->BottomControlsMinHeight());
}

}  // namespace cc
