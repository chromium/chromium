// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_OVERSCROLL_BEHAVIOR_H_
#define CC_INPUT_OVERSCROLL_BEHAVIOR_H_

#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT OverscrollBehavior {
  enum class Type {
    // Same as contain but also hint that no overscroll affordance should be
    // triggered.
    kNone,
    // Allows the default behavior for the user agent.
    kAuto,
    // Hint to disable scroll chaining. The user agent may show an appropriate
    // overscroll affordance.
    kContain,
    kMax = kContain
  };

  OverscrollBehavior() : x(Type::kAuto), y(Type::kAuto) {}
  explicit OverscrollBehavior(Type type) : x(type), y(type) {}
  OverscrollBehavior(Type x_type, Type y_type) : x(x_type), y(y_type) {}

  Type x;
  Type y;

  bool operator==(const OverscrollBehavior& a) const {
    return (a.x == x) && (a.y == y);
  }
  bool operator!=(const OverscrollBehavior& a) const { return !(*this == a); }
};

}  // namespace cc

#endif  // CC_INPUT_OVERSCROLL_BEHAVIOR_H_
