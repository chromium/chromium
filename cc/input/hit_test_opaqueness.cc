// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/input/hit_test_opaqueness.h"
#include "base/check_op.h"
#include "base/notreached.h"

namespace cc {

namespace {

// Returns true if UnionRects(rect1, rect2) doesn't have any area not covered
// by either rect1 or rect2.
bool UnionIsTight(const gfx::Rect& rect1, const gfx::Rect& rect2) {
  return gfx::UnionRects(rect1, rect2) == gfx::MaximumCoveredRect(rect1, rect2);
}

}  // anonymous namespace

const char* HitTestOpaquenessToString(HitTestOpaqueness opaqueness) {
  return opaqueness == HitTestOpaqueness::kTransparent ? "transparent"
         : opaqueness == HitTestOpaqueness::kOpaque    ? "opaque"
                                                       : "mixed";
}

HitTestOpaqueness UnionHitTestOpaqueness(const gfx::Rect& rect1,
                                         HitTestOpaqueness opaqueness1,
                                         const gfx::Rect& rect2,
                                         HitTestOpaqueness opaqueness2) {
  if (rect1.IsEmpty()) {
    return opaqueness2;
  }
  if (rect2.IsEmpty()) {
    return opaqueness1;
  }
  if (opaqueness1 < opaqueness2) {
    return UnionHitTestOpaqueness(rect2, opaqueness2, rect1, opaqueness1);
  }

  switch (opaqueness1) {
    case HitTestOpaqueness::kTransparent:
      DCHECK_EQ(opaqueness2, HitTestOpaqueness::kTransparent);
      return HitTestOpaqueness::kTransparent;
    case HitTestOpaqueness::kMixed:
      DCHECK_NE(opaqueness2, HitTestOpaqueness::kOpaque);
      return HitTestOpaqueness::kMixed;
    case HitTestOpaqueness::kOpaque:
      if (opaqueness2 == HitTestOpaqueness::kOpaque) {
        return UnionIsTight(rect1, rect2) ? HitTestOpaqueness::kOpaque
                                          : HitTestOpaqueness::kMixed;
      }
      return rect1.Contains(rect2) ? HitTestOpaqueness::kOpaque
                                   : HitTestOpaqueness::kMixed;
  }
  NOTREACHED();
}

}  // namespace cc
