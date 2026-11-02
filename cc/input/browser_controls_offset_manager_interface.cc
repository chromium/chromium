// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/browser_controls_offset_manager_interface.h"

#include <algorithm>

#include "base/check.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_ref.h"
#include "build/build_config.h"
#include "cc/input/browser_controls_offset_manager_client.h"
#include "cc/input/browser_controls_offset_tag_modifications.h"
#include "cc/input/browser_controls_state.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

BrowserControlsOffsetManagerInterface::BrowserControlsOffsetManagerInterface(
    BrowserControlsOffsetManagerClient* client)
    : client_(client) {
  CHECK(client_);
}

BrowserControlsOffsetManagerInterface::
    ~BrowserControlsOffsetManagerInterface() = default;

float BrowserControlsOffsetManagerInterface::ControlsTopOffset() const {
  return ContentTopOffset() - TopControlsHeight();
}

float BrowserControlsOffsetManagerInterface::ContentTopOffset() const {
  return TopControlsHeight() > 0 ? TopControlsShownRatio() * TopControlsHeight()
                                 : 0.0f;
}

float BrowserControlsOffsetManagerInterface::TopControlsShownRatio() const {
  return client_->CurrentTopControlsShownRatio();
}

float BrowserControlsOffsetManagerInterface::TopControlsHeight() const {
  return client_->TopControlsHeight();
}

float BrowserControlsOffsetManagerInterface::TopControlsMinHeight() const {
  return client_->TopControlsMinHeight();
}

float BrowserControlsOffsetManagerInterface::TopControlsMinShownRatio() const {
  return TopControlsHeight() ? TopControlsMinHeight() / TopControlsHeight()
                             : 0.0f;
}

float BrowserControlsOffsetManagerInterface::BottomControlsHeight() const {
  return client_->BottomControlsHeight();
}

float BrowserControlsOffsetManagerInterface::BottomControlsMinHeight() const {
  return client_->BottomControlsMinHeight();
}

float BrowserControlsOffsetManagerInterface::BottomControlsMinShownRatio()
    const {
  return BottomControlsHeight()
             ? BottomControlsMinHeight() / BottomControlsHeight()
             : 0.0f;
}

float BrowserControlsOffsetManagerInterface::ContentBottomOffset() const {
  return BottomControlsHeight() > 0
             ? BottomControlsShownRatio() * BottomControlsHeight()
             : 0.0f;
}

float BrowserControlsOffsetManagerInterface::BottomControlsShownRatio() const {
  return client_->CurrentBottomControlsShownRatio();
}

float BrowserControlsOffsetManagerInterface::TopControlsMinHeightOffset()
    const {
  return top_controls_min_height_offset_;
}

float BrowserControlsOffsetManagerInterface::BottomControlsMinHeightOffset()
    const {
  return bottom_controls_min_height_offset_;
}

BrowserControlsMetadata BrowserControlsOffsetManagerInterface::GetMetadata()
    const {
  BrowserControlsMetadata metadata;
  metadata.top_controls_height = TopControlsHeight();
  metadata.top_controls_shown_ratio =
      std::clamp(TopControlsShownRatio(), 0.0f, 1.0f);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  metadata.bottom_controls_height = BottomControlsHeight();
  metadata.bottom_controls_shown_ratio =
      std::clamp(BottomControlsShownRatio(), 0.0f, 1.0f);
  metadata.top_controls_min_height_offset = TopControlsMinHeightOffset();
  metadata.bottom_controls_min_height_offset = BottomControlsMinHeightOffset();
  metadata.has_offset_tag = !offset_tag_modifications_.tags.IsEmpty();
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return metadata;
}

void BrowserControlsOffsetManagerInterface::UpdateBrowserControlsState(
    BrowserControlsState constraints,
    BrowserControlsState current,
    bool animate,
    base::optional_ref<const BrowserControlsOffsetTagModifications>
        offset_tag_modifications) {
  DCHECK(!(constraints == BrowserControlsState::kShown &&
           current == BrowserControlsState::kHidden));
  DCHECK(!(constraints == BrowserControlsState::kHidden &&
           current == BrowserControlsState::kShown));

  TRACE_EVENT2(
      "cc", "BrowserControlsOffsetManagerInterface::UpdateBrowserControlsState",
      "constraints", static_cast<int>(constraints), "current",
      static_cast<int>(current));

  // If the constraints have changed we need to inform Blink about it since
  // that'll affect main thread scrolling as well as layout.
  if (permitted_state_ != constraints) {
    constraint_changed_since_commit_ = true;
    client_->SetNeedsCommit();
  }

  permitted_state_ = constraints;

  if (offset_tag_modifications.has_value()) {
    offset_tag_modifications_ = offset_tag_modifications.value();
  }

  OnBrowserControlsStateUpdated(constraints, current, animate);
}

BrowserControlsState
BrowserControlsOffsetManagerInterface::PullConstraintForMainThread(
    bool* out_changed_since_commit) {
  DCHECK(out_changed_since_commit);
  *out_changed_since_commit = constraint_changed_since_commit_;
  return permitted_state_;
}

void BrowserControlsOffsetManagerInterface::
    NotifyConstraintSyncedToMainThread() {
  constraint_changed_since_commit_ = false;
}

double BrowserControlsOffsetManagerInterface::PredictViewportBoundsDelta(
    double current_bounds_delta,
    gfx::Vector2dF scroll_distance) {
  double adjustment = current_bounds_delta;
  if (scroll_distance.y() > 0 && adjustment > 0) {
    // We're scrolling down and started to hide controls. Let's assume they're
    // going to be fully hidden by the end of the fling.
    if (TopControlsShownRatio() < 1) {
      adjustment += ContentTopOffset();
    }
    if (BottomControlsShownRatio() < 1) {
      adjustment += ContentBottomOffset();
    }
  }
  if (scroll_distance.y() < 0 && adjustment < 0) {
    // We're scrolling up and started to show controls. Let's assume they're
    // going to be fully shown by the end of the fling.
    if (TopControlsShownRatio() > 0) {
      adjustment -= TopControlsHeight() - ContentTopOffset();
    }
    if (BottomControlsShownRatio() > 0) {
      adjustment -= BottomControlsHeight() - ContentBottomOffset();
    }
  }
  return adjustment;
}

}  // namespace cc
