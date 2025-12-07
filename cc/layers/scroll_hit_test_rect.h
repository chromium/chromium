// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SCROLL_HIT_TEST_RECT_H_
#define CC_LAYERS_SCROLL_HIT_TEST_RECT_H_

#include "cc/paint/element_id.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// Stores the element id of a scroll node and its hit test rect in layer space.
struct ScrollHitTestRect {
  ElementId scroll_element_id;
  gfx::Rect hit_test_rect;

  bool operator==(const ScrollHitTestRect& other) const = default;
};

}  // namespace cc

#endif  // CC_LAYERS_SCROLL_HIT_TEST_RECT_H_
