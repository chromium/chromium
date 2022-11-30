// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_

#include "ash/ash_export.h"

namespace ash {
namespace network_icon {

// Observer interface class for animating network icons.
class ASH_EXPORT AnimationObserver {
 public:
  // Called when the image has changed due to animation.
  virtual void NetworkIconChanged() = 0;

 protected:
  virtual ~AnimationObserver() {}
};

}  // namespace network_icon
}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
