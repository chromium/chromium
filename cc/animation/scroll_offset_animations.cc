// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/scroll_offset_animations.h"

#include "cc/animation/animation_host.h"

namespace cc {

ScrollOffsetAnimationUpdate::ScrollOffsetAnimationUpdate() = default;

ScrollOffsetAnimationUpdate::ScrollOffsetAnimationUpdate(ElementId element_id)
    : element_id_(element_id), takeover_(false) {}

ScrollOffsetAnimations::ScrollOffsetAnimations(AnimationHost* animation_host)
    : animation_host_(animation_host) {}

ScrollOffsetAnimations::~ScrollOffsetAnimations() = default;

ScrollOffsetAnimationUpdate ScrollOffsetAnimations::GetUpdateForElementId(
    ElementId element_id) const {
  DCHECK(element_id);
  auto iter = element_to_update_map_.find(element_id);
  return iter == element_to_update_map_.end()
             ? ScrollOffsetAnimationUpdate(element_id)
             : iter->second;
}

void ScrollOffsetAnimations::AddAdjustmentUpdate(ElementId element_id,
                                                 gfx::Vector2dF adjustment) {
  DCHECK(element_id);
  ScrollOffsetAnimationUpdate update = GetUpdateForElementId(element_id);
  update.adjustment_ += adjustment;
  element_to_update_map_[element_id] = update;
  animation_host_->SetNeedsCommit();
  animation_host_->SetNeedsPushProperties();
}

void ScrollOffsetAnimations::AddTakeoverUpdate(ElementId element_id) {
  DCHECK(element_id);
  ScrollOffsetAnimationUpdate update = GetUpdateForElementId(element_id);
  update.takeover_ = true;
  element_to_update_map_[element_id] = update;
  animation_host_->SetNeedsCommit();
  animation_host_->SetNeedsPushProperties();
}

bool ScrollOffsetAnimations::HasUpdatesForTesting() const {
  return !element_to_update_map_.empty();
}

void ScrollOffsetAnimations::PushPropertiesTo(
    ScrollOffsetAnimationsImpl* animations) {
  DCHECK(animations);
  if (element_to_update_map_.empty())
    return;

  for (auto& kv : element_to_update_map_) {
    const auto& update = kv.second;
    if (update.takeover_)
      animations->ScrollAnimationAbort(true /*needs_completion*/);
    else
      animations->ScrollAnimationApplyAdjustment(update.element_id_,
                                                 update.adjustment_);
  }
  element_to_update_map_.clear();
}

}  // namespace cc
