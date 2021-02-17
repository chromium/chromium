// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/property_animation_state.h"


namespace cc {

PropertyAnimationState::PropertyAnimationState() = default;

PropertyAnimationState::PropertyAnimationState(
    const PropertyAnimationState& rhs) = default;

PropertyAnimationState::~PropertyAnimationState() = default;

bool PropertyAnimationState::operator==(
    const PropertyAnimationState& other) const {
  return currently_running == other.currently_running &&
         potentially_animating == other.potentially_animating;
}

bool PropertyAnimationState::operator!=(
    const PropertyAnimationState& other) const {
  return !operator==(other);
}

PropertyAnimationState& PropertyAnimationState::operator|=(
    const PropertyAnimationState& other) {
  currently_running |= other.currently_running;
  potentially_animating |= other.potentially_animating;

  return *this;
}

PropertyAnimationState& PropertyAnimationState::operator^=(
    const PropertyAnimationState& other) {
  currently_running ^= other.currently_running;
  potentially_animating ^= other.potentially_animating;

  return *this;
}

PropertyAnimationState& PropertyAnimationState::operator&=(
    const PropertyAnimationState& other) {
  currently_running &= other.currently_running;
  potentially_animating &= other.potentially_animating;

  return *this;
}

PropertyAnimationState operator^(const PropertyAnimationState& lhs,
                                 const PropertyAnimationState& rhs) {
  PropertyAnimationState result = lhs;
  result ^= rhs;
  return result;
}

bool PropertyAnimationState::IsValid() const {
  // currently_running must be a subset for potentially_animating.
  // currently <= potentially i.e. potentially || !currently.
  TargetProperties result = potentially_animating | ~currently_running;
  return result.all();
}

void PropertyAnimationState::Clear() {
  currently_running.reset();
  potentially_animating.reset();
}

}  // namespace cc
