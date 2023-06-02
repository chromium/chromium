// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_OBSERVER_H_
#define ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/observer_list_types.h"

namespace ash {

// Observer interface class for animating network icons.
class ASH_EXPORT HotspotIconAnimationObserver : public base::CheckedObserver {
 public:
  // Called when the image has changed due to animation.
  virtual void HotspotIconChanged() = 0;

 protected:
  HotspotIconAnimationObserver() = default;
  ~HotspotIconAnimationObserver() override = default;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOTSPOT_HOTSPOT_ICON_ANIMATION_OBSERVER_H_
