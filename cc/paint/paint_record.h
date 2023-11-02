// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORD_H_
#define CC_PAINT_PAINT_RECORD_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {
class ImageProvider;

// TODO(enne): Don't want to rename the world for this.  Using these as the
// same types for now prevents an extra allocation.  Probably PaintRecord
// will become an interface in the future.
using PaintRecord = PaintOpBuffer;

// TODO(enne): Remove these if possible, they are really expensive.
CC_PAINT_EXPORT sk_sp<SkPicture> ToSkPicture(
    sk_sp<PaintRecord> record,
    const SkRect& bounds,
    ImageProvider* image_provider = nullptr,
    PlaybackParams::CustomDataRasterCallback callback =
        PlaybackParams::CustomDataRasterCallback());

CC_PAINT_EXPORT sk_sp<const SkPicture> ToSkPicture(
    sk_sp<const PaintRecord> record,
    const SkRect& bounds,
    ImageProvider* image_provider = nullptr,
    PlaybackParams::CustomDataRasterCallback callback =
        PlaybackParams::CustomDataRasterCallback());

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORD_H_
