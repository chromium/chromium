// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_ID_H_
#define CC_PAINT_IMAGE_ID_H_

#include <stdint.h>

#include "base/containers/flat_set.h"
#include "cc/paint/paint_image.h"

namespace cc {

using PaintImageIdFlatSet = base::flat_set<PaintImage::Id>;

}  // namespace cc

#endif  // CC_PAINT_IMAGE_ID_H_
