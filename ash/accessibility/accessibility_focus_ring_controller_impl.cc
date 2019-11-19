// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_focus_ring_controller_impl.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_cursor_ring_layer.h"
#include "ash/accessibility/accessibility_focus_ring_group.h"
#include "ash/accessibility/accessibility_focus_ring_layer.h"
#include "ash/accessibility/accessibility_highlight_layer.h"
#include "ash/accessibility/focus_ring_layer.h"
#include "ash/accessibility/layer_animation_info.h"
#include "base/logging.h"

namespace ash {

namespace {

// Cursor constants.
constexpr int kCursorFadeInTimeMilliseconds = 400;
constexpr int kCursorFadeOutTimeMilliseconds = 1200;
constexpr int kCursorRingColorRed = 255;
constexpr int kCursorRingColorGreen = 51;
constexpr int kCursorRingColorBlue = 51;

// Caret constants.
constexpr int kCaretFadeInTimeMilliseconds = 100;
constexpr int kCaretFadeOutTimeMilliseconds = 1600;
constexpr int kCaretRingColorRed = 51;
constexpr int kCaretRingColorGreen = 51;
constexpr int kCaretRingColorBlue = 255;

// Highlight constants.
constexpr float kHighlightOpacity = 0.3f;

}  // namespace

AccessibilityFocusRingControllerImpl::AccessibilityFocusRingControllerImpl() {
  cursor_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kCursorFadeInTimeMilliseconds);
  cursor_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kCursorFadeOutTimeMilliseconds);
  caret_animation_info_.fade_in_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeInTimeMilliseconds);
  caret_animation_info_.fade_out_time =
      base::TimeDelta::FromMilliseconds(kCaretFadeOutTimeMilliseconds);
}

AccessibilityFocusRingControllerImpl::~AccessibilityFocusRingControllerImpl() =
    default;

void AccessibilityFocusRingControllerImpl::SetFocusRing(
    const std::string& focus_ring_id,
    std::unique_ptr<AccessibilityFocusRingInfo> focus_ring) {
  // This code assumes |focus_ring| is always non-null.
  DCHECK(focus_ring);
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForId(focus_ring_id, true /* Create if missing */);
  if (focus_ring_group->UpdateFocusRing(std::move(focus_ring), this))
    OnLayerChange(focus_ring_group->focus_animation_info());
}

void AccessibilityFocusRingControllerImpl::HideFocusRing(
    const std::string& focus_ring_id) {
  AccessibilityFocusRingGroup* focus_ring_group =
      GetFocusRingGroupForId(focus_ring_id, false /* Do not create */);
  if (!focus_ring_group)
    return;
  focus_ring_group->ClearFocusRects(this);
  OnLayerChange(focus_ring_group->focus_animation_info());
}

void AccessibilityFocusRingControllerImpl::SetHighlights(
    const std::vector<gfx::Rect>& rects,
    SkColor color) {
  highlight_rects_ = rects;
  GetColorAndOpacityFromColor(color, kHighlightOpacity, &highlight_color_,
                              &highlight_opacity_);
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingControllerImpl::HideHighlights() {
  highlight_rects_.clear();
  UpdateHighlightFromHighlightRects();
}

void AccessibilityFocusRingControllerImpl::UpdateHighlightFromHighlightRects() {
  if (!highlight_layer_)
    highlight_layer_ = std::make_unique<AccessibilityHighlightLayer>(this);
  highlight_layer_->Set(highlight_rects_, highlight_color_);
  highlight_layer_->SetOpacity(highlight_opacity_);
}

void AccessibilityFocusRingControllerImpl::OnLayerChange(
    LayerAnimationInfo* animation_info) {
  animation_info->change_time = base::TimeTicks::Now();
  if (animation_info->opacity == 0)
    animation_info->start_time = animation_info->change_time;
}

void AccessibilityFocusRingControllerImpl::SetCursorRing(
    const gfx::Point& location) {
  cursor_location_ = location;
  if (!cursor_layer_) {
    cursor_layer_.reset(new AccessibilityCursorRingLayer(
        this, kCursorRingColorRed, kCursorRingColorGreen,
        kCursorRingColorBlue));
  }
  cursor_layer_->Set(location);
  OnLayerChange(&cursor_animation_info_);
}

void AccessibilityFocusRingControllerImpl::HideCursorRing() {
  cursor_layer_.reset();
}

void AccessibilityFocusRingControllerImpl::SetCaretRing(
    const gfx::Point& location) {
  caret_location_ = location;

  if (!caret_layer_) {
    caret_layer_.reset(new AccessibilityCursorRingLayer(
        this, kCaretRingColorRed, kCaretRingColorGreen, kCaretRingColorBlue));
  }

  caret_layer_->Set(location);
  OnLayerChange(&caret_animation_info_);
}

void AccessibilityFocusRingControllerImpl::HideCaretRing() {
  caret_layer_.reset();
}

void AccessibilityFocusRingControllerImpl::SetNoFadeForTesting() {
  no_fade_for_testing_ = true;
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter) {
    iter->second->set_no_fade_for_testing();
    iter->second->focus_animation_info()->fade_in_time = base::TimeDelta();
    iter->second->focus_animation_info()->fade_out_time =
        base::TimeDelta::FromHours(1);
  }
  cursor_animation_info_.fade_in_time = base::TimeDelta();
  cursor_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
  caret_animation_info_.fade_in_time = base::TimeDelta();
  caret_animation_info_.fade_out_time = base::TimeDelta::FromHours(1);
}

const AccessibilityFocusRingGroup*
AccessibilityFocusRingControllerImpl::GetFocusRingGroupForTesting(
    const std::string& focus_ring_id) {
  return GetFocusRingGroupForId(focus_ring_id, false /* create if missing */);
}

void AccessibilityFocusRingControllerImpl::GetColorAndOpacityFromColor(
    SkColor color,
    float default_opacity,
    SkColor* result_color,
    float* result_opacity) {
  int alpha = SkColorGetA(color);
  if (alpha == 0xFF) {
    *result_opacity = default_opacity;
  } else {
    *result_opacity = SkColor4f::FromColor(color).fA;
  }
  *result_color = SkColorSetA(color, 0xFF);
}

void AccessibilityFocusRingControllerImpl::OnDeviceScaleFactorChanged() {
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter)
    iter->second->UpdateFocusRingsFromInfo(this);
}

void AccessibilityFocusRingControllerImpl::OnAnimationStep(
    base::TimeTicks timestamp) {
  for (auto iter = focus_ring_groups_.begin(); iter != focus_ring_groups_.end();
       ++iter) {
    if (iter->second->CanAnimate())
      iter->second->AnimateFocusRings(timestamp);
  }

  if (cursor_layer_ && cursor_layer_->CanAnimate())
    AnimateCursorRing(timestamp);

  if (caret_layer_ && caret_layer_->CanAnimate())
    AnimateCaretRing(timestamp);
}

void AccessibilityFocusRingControllerImpl::AnimateCursorRing(
    base::TimeTicks timestamp) {
  CHECK(cursor_layer_);

  ComputeOpacity(&cursor_animation_info_, timestamp);
  if (cursor_animation_info_.opacity == 0.0) {
    cursor_layer_.reset();
    return;
  }
  cursor_layer_->SetOpacity(cursor_animation_info_.opacity);
}

void AccessibilityFocusRingControllerImpl::AnimateCaretRing(
    base::TimeTicks timestamp) {
  CHECK(caret_layer_);

  ComputeOpacity(&caret_animation_info_, timestamp);
  if (caret_animation_info_.opacity == 0.0) {
    caret_layer_.reset();
    return;
  }
  caret_layer_->SetOpacity(caret_animation_info_.opacity);
}

AccessibilityFocusRingGroup*
AccessibilityFocusRingControllerImpl::GetFocusRingGroupForId(
    const std::string& focus_ring_id,
    bool create) {
  auto iter = focus_ring_groups_.find(focus_ring_id);
  if (iter != focus_ring_groups_.end())
    return iter->second.get();

  if (!create)
    return nullptr;

  // Add it and then return it.
  focus_ring_groups_[focus_ring_id] =
      std::make_unique<AccessibilityFocusRingGroup>();
  if (no_fade_for_testing_)
    SetNoFadeForTesting();

  return focus_ring_groups_[focus_ring_id].get();
}

}  // namespace ash
