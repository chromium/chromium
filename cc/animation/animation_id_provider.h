// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_ID_PROVIDER_H_
#define CC_ANIMATION_ANIMATION_ID_PROVIDER_H_

#include "cc/animation/animation_export.h"

namespace cc {

class CC_ANIMATION_EXPORT AnimationIdProvider {
 public:
  AnimationIdProvider() = delete;

  // These functions each return monotonically increasing values.
  static int NextKeyframeModelId();
  static int NextGroupId();
  static int NextTimelineId();
  static int NextAnimationId();
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_ID_PROVIDER_H_
