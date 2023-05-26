// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_icon_animation.h"

#include "ash/system/hotspot/hotspot_icon_animation_observer.h"

namespace ash {

HotspotIconAnimation::HotspotIconAnimation() {
  // Set up the animation throbber.
  animation_.SetThrobDuration(base::Seconds(1));
  animation_.SetTweenType(gfx::Tween::LINEAR);
}

HotspotIconAnimation::~HotspotIconAnimation() {
  CHECK(observers_.empty());
}

void HotspotIconAnimation::AnimationProgressed(
    const gfx::Animation* animation) {
  if (animation != &animation_) {
    return;
  }
  for (HotspotIconAnimationObserver& observer : observers_) {
    observer.HotspotIconChanged();
  }
}

double HotspotIconAnimation::GetAnimation() {
  if (!animation_.is_animating()) {
    animation_.Reset();
    animation_.StartThrobbing(/*throb indefinitely=*/-1);
    return 0;
  }
  return animation_.GetCurrentValue();
}

void HotspotIconAnimation::AddObserver(HotspotIconAnimationObserver* observer) {
  if (!observers_.HasObserver(observer)) {
    observers_.AddObserver(observer);
  }
}

void HotspotIconAnimation::RemoveObserver(
    HotspotIconAnimationObserver* observer) {
  if (observers_.HasObserver(observer)) {
    observers_.RemoveObserver(observer);
  }
  if (observers_.empty()) {
    animation_.Reset();  // Stops the animation and resets the current value.
  }
}

}  // namespace ash
