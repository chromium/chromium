// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_cib.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/time/time.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_state.h"
#include "components/viz/common/quads/offset_tag.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

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
  return false;
}

void BrowserControlsOffsetManagerCib::OnBrowserControlsStateUpdated(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate) {}

void BrowserControlsOffsetManagerCib::OnBrowserControlsParamsChanged(
    bool animate_changes) {}

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
  return gfx::Vector2dF();
}

void BrowserControlsOffsetManagerCib::ResetAnimations() {}

}  // namespace cc
