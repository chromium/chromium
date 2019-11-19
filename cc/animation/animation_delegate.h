// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_DELEGATE_H_
#define CC_ANIMATION_ANIMATION_DELEGATE_H_

#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/keyframe_model.h"

namespace cc {

class CC_ANIMATION_EXPORT AnimationDelegate {
 public:
  // TODO(yigu): The Notify* methods will be called multiple times per
  // animation (once for effect/property pairing).
  // Ideally, we would only notify start once (e.g., wait on all effects to
  // start before notifying delegate) this way effect becomes an internal
  // details of the animation. Perhaps we can do that at some point maybe as
  // part of https://bugs.chromium.org/p/chromium/issues/detail?id=810003
  virtual void NotifyAnimationStarted(base::TimeTicks monotonic_time,
                                      int target_property,
                                      int group) = 0;
  virtual void NotifyAnimationFinished(base::TimeTicks monotonic_time,
                                       int target_property,
                                       int group) = 0;

  virtual void NotifyAnimationAborted(base::TimeTicks monotonic_time,
                                      int target_property,
                                      int group) = 0;

  virtual void NotifyAnimationTakeover(
      base::TimeTicks monotonic_time,
      int target_property,
      base::TimeTicks animation_start_time,
      std::unique_ptr<AnimationCurve> curve) = 0;
  virtual void NotifyLocalTimeUpdated(
      base::Optional<base::TimeDelta> local_time) = 0;

 protected:
  virtual ~AnimationDelegate() {}
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_DELEGATE_H_
