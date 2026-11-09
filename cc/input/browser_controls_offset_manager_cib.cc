// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_cib.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/time/time.h"
#include "cc/input/browser_controls_animation.h"
#include "cc/input/browser_controls_animation_params.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_state.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

namespace {

// Schedules `animation` towards `target_visible_height / controls_height` over
// `duration` using `curve` so the shown-ratio update and viewport
// counter-scroll occur in the same impl frame.
void StartAnimation(BrowserControlsAnimation& animation,
                    float controls_height,
                    float current_ratio,
                    const std::pair<float, float>& ratio_range,
                    float target_visible_height,
                    base::TimeDelta duration,
                    const BrowserControlsAnimation::Curve& curve) {
  // When a control has zero height its shown ratio does not affect layout, so
  // keep the current ratio instead of dividing by zero.
  if (controls_height <= 0.0f) {
    return;
  }

  const float target_ratio = std::clamp(target_visible_height / controls_height,
                                        ratio_range.first, ratio_range.second);
  if (current_ratio == target_ratio) {
    return;
  }

  // Passing `target_ratio` for both start and stop resolves in one frame at
  // zero duration because `Tick()` evaluates the start value at `start_time_`
  // and `IsComplete()` finishes immediately.
  const float start_ratio = duration.is_zero() ? target_ratio : current_ratio;
  animation.Initialize(
      current_ratio < target_ratio
          ? BrowserControlsAnimation::Direction::kShowingControls
          : BrowserControlsAnimation::Direction::kHidingControls,
      start_ratio, target_ratio, duration,
      /*jump_to_end_on_reset=*/false, curve);
}

}  // namespace

BrowserControlsOffsetManagerCib::BrowserControlsOffsetManagerCib(
    BrowserControlsOffsetManagerClient* client)
    : BrowserControlsOffsetManagerInterface(client) {}

BrowserControlsOffsetManagerCib::~BrowserControlsOffsetManagerCib() = default;

std::vector<viz::OffsetTagValue>
BrowserControlsOffsetManagerCib::GetOffsetTagValues() const {
  // TODO(crbug.com/524720687): Remove content_offset_tag once
  // kControlsInBrowser is enabled by default and renderer input and hit
  // testing no longer assume web content starts at the origin.
  if (TopControlsHeight() <= 0) {
    return {};
  }
  const viz::OffsetTag& content_offset_tag =
      GetOffsetTagModifications().tags.content_offset_tag;
  if (!content_offset_tag) {
    return {};
  }
  // ViewAndroid::OnTopControlsChanged() also rounds the offset before handing
  // it off to Android.
  const float offset = TopControlsHeight() - ContentTopOffset();
  return {{.tag = content_offset_tag,
           .offset = gfx::Vector2dF(0.0f, -std::round(offset))}};
}

std::pair<float, float>
BrowserControlsOffsetManagerCib::TopControlsShownRatioRange() {
  return std::make_pair(0.0f, 1.0f);
}

std::pair<float, float>
BrowserControlsOffsetManagerCib::BottomControlsShownRatioRange() {
  return std::make_pair(0.0f, 1.0f);
}

bool BrowserControlsOffsetManagerCib::HasAnimation() {
  return top_controls_animation_.IsInitialized() ||
         bottom_controls_animation_.IsInitialized();
}

void BrowserControlsOffsetManagerCib::OnBrowserControlsStateUpdated(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {}

void BrowserControlsOffsetManagerCib::OnBrowserControlsParamsChanged(
    bool animate_changes) {}

void BrowserControlsOffsetManagerCib::SetAnimation(
    const BrowserControlsAnimationParams& params) {
  CHECK(!params.duration.is_negative());
  ResetAnimations();

  const BrowserControlsAnimation::Curve curve(params.curve);
  StartAnimation(top_controls_animation_, TopControlsHeight(),
                 TopControlsShownRatio(), TopControlsShownRatioRange(),
                 params.target_top_controls_visible_height, params.duration,
                 curve);
  StartAnimation(bottom_controls_animation_, BottomControlsHeight(),
                 BottomControlsShownRatio(), BottomControlsShownRatioRange(),
                 params.target_bottom_controls_visible_height, params.duration,
                 curve);

  if (HasAnimation()) {
    client().DidChangeBrowserControlsPosition();
  }
}

void BrowserControlsOffsetManagerCib::ScrollBegin() {}

gfx::Vector2dF BrowserControlsOffsetManagerCib::ScrollBy(
    const gfx::Vector2dF& pending_delta,
    bool is_inertial) {
  return pending_delta;
}

void BrowserControlsOffsetManagerCib::ScrollEnd(
    const gfx::Vector2dF& compensated_scroll_delta) {}

void BrowserControlsOffsetManagerCib::PinchBegin() {}

void BrowserControlsOffsetManagerCib::PinchEnd() {}

gfx::Vector2dF BrowserControlsOffsetManagerCib::Animate(
    base::TimeTicks monotonic_time) {
  const std::optional<float> top_ratio =
      top_controls_animation_.Tick(monotonic_time);
  const std::optional<float> bottom_ratio =
      bottom_controls_animation_.Tick(monotonic_time);
  if (!top_ratio && !bottom_ratio) {
    return gfx::Vector2dF();
  }

  // Counter-scroll the page by the change in the top controls visible height so
  // that the web contents stay in place on screen. The caller applies the
  // returned delta in this frame.
  const float old_content_top_offset = ContentTopOffset();
  client().SetCurrentBrowserControlsShownRatio(
      top_ratio.value_or(TopControlsShownRatio()),
      bottom_ratio.value_or(BottomControlsShownRatio()));
  return gfx::Vector2dF(0.0f, ContentTopOffset() - old_content_top_offset);
}

void BrowserControlsOffsetManagerCib::ResetAnimations() {
  top_controls_animation_.Reset();
  bottom_controls_animation_.Reset();
}

}  // namespace cc
