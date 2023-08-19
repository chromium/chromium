// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_HIT_TEST_OPAQUENESS_H_
#define CC_INPUT_HIT_TEST_OPAQUENESS_H_

#include <stdint.h>

#include "cc/cc_export.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// Describes the opaqueness for hit testing in a layered content e.g. a layer.
enum class HitTestOpaqueness : uint8_t {
  // The whole layered content is transparent (i.e. as if it didn't exist)
  // to hit test.
  kTransparent,
  // Some areas may be transparent, while some may be opaque.
  kMixed,
  // The whole layered content is opaque to hit test.
  kOpaque,
};

CC_EXPORT const char* HitTestOpaquenessToString(HitTestOpaqueness opaqueness);

// Returns the hit test opaqueness of the bounds containing `rect1` and `rect2`
// of specified opaqueness.
CC_EXPORT HitTestOpaqueness
UnionHitTestOpaqueness(const gfx::Rect& rect1,
                       HitTestOpaqueness opaqueness1,
                       const gfx::Rect& rect2,
                       HitTestOpaqueness opaqueness2);

}  // namespace cc

#endif  // CC_INPUT_HIT_TEST_OPAQUENESS_H_
