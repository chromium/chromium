// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_ID_H_
#define CC_PAINT_IMAGE_ID_H_

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "cc/paint/element_id.h"
#include "cc/paint/paint_image.h"

namespace cc {

using PaintImageIdFlatSet = base::flat_set<PaintImage::Id>;

// Represents image animation state for an unique PaintImage. The first field
// indicates whether any drivers require the animation to be ticked; the second
// field records client elements that should be informed when the animation
// advances.
using AnimatedImageDriverState = std::pair<bool, std::vector<ElementId>>;
using AnimatedImageDriverMap =
    base::flat_map<PaintImage::Id, AnimatedImageDriverState>;

}  // namespace cc

#endif  // CC_PAINT_IMAGE_ID_H_
