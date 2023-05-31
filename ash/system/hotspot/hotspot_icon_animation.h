// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_H_

#include "ash/ash_export.h"
#include "base/observer_list.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/throb_animation.h"

namespace ash {

class HotspotIconAnimationObserver;

// Single instance class to handle icon animations and keep them in sync.
class ASH_EXPORT HotspotIconAnimation : public gfx::AnimationDelegate {
 public:
  HotspotIconAnimation();
  ~HotspotIconAnimation() override;

  // Returns the current animation value, [0-1].
  double GetAnimation();

  // The animation stops when all observers have been removed.
  // Be sure to remove observers when no associated icons are animating.
  void AddObserver(HotspotIconAnimationObserver* observer);
  void RemoveObserver(HotspotIconAnimationObserver* observer);

  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;

 private:
  gfx::ThrobAnimation animation_{this};
  base::ObserverList<HotspotIconAnimationObserver> observers_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_H_
