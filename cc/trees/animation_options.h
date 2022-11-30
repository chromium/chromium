// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_ANIMATION_OPTIONS_H_
#define CC_TREES_ANIMATION_OPTIONS_H_

#include <memory>

#include "cc/cc_export.h"

namespace cc {

// This class acts as opaque handle in cc while the actual implementation lives
// in blink. It is meant to facilitate plumbing the options from blink main
// thread to worklet thread via cc animations machinery.
class CC_EXPORT AnimationOptions {
 public:
  virtual ~AnimationOptions() = default;
  virtual std::unique_ptr<AnimationOptions> Clone() const = 0;
};

}  // namespace cc

#endif  // CC_TREES_ANIMATION_OPTIONS_H_
