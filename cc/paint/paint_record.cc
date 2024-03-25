// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_record.h"

#include <algorithm>
#include <utility>

#include "base/no_destructor.h"
#include "cc/paint/paint_op_buffer_iterator.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkRect.h"

namespace cc {

PaintRecord::PaintRecord() {
  static base::NoDestructor<sk_sp<PaintOpBuffer>> empty_buffer(
      sk_make_sp<PaintOpBuffer>());
  buffer_ = *empty_buffer;
}

PaintRecord::~PaintRecord() = default;
PaintRecord::PaintRecord(PaintRecord&&) = default;
PaintRecord& PaintRecord::operator=(PaintRecord&&) = default;
PaintRecord::PaintRecord(const PaintRecord&) = default;
PaintRecord& PaintRecord::operator=(const PaintRecord&) = default;

PaintRecord::PaintRecord(sk_sp<PaintOpBuffer> buffer)
    : buffer_(std::move(buffer)) {
  CHECK(buffer_);
}

// static
SkRect PaintRecord::GetFixedScaleBounds(const SkMatrix& ctm,
                                        const SkRect& bounds,
                                        int max_texture_size) {
  SkSize scale;
  if (!ctm.decomposeScale(&scale)) {
    // Decomposition failed, use an approximation.
    scale.set(SkScalarSqrt(ctm.getScaleX() * ctm.getScaleX() +
                           ctm.getSkewX() * ctm.getSkewX()),
              SkScalarSqrt(ctm.getScaleY() * ctm.getScaleY() +
                           ctm.getSkewY() * ctm.getSkewY()));
  }

  SkScalar raster_width = bounds.width() * scale.width();
  SkScalar raster_height = bounds.height() * scale.height();
  SkScalar tile_area = raster_width * raster_height;
  // Clamp the tile area to about 4M pixels, and per-dimension max texture size
  // if it's provided.
  static const SkScalar kMaxTileArea = 2048 * 2048;
  SkScalar down_scale = 1.f;
  if (tile_area > kMaxTileArea) {
    down_scale = SkScalarSqrt(kMaxTileArea / tile_area);
  }
  if (max_texture_size > 0) {
    // This only updates down_scale if the tile is larger than the texture size
    // after ensuring its area is less than kMaxTileArea
    down_scale = std::min(
        down_scale, max_texture_size / std::max(raster_width, raster_height));
  }

  if (down_scale < 1.f) {
    scale.set(down_scale * scale.width(), down_scale * scale.height());
  }
  return SkRect::MakeXYWH(
      bounds.fLeft * scale.width(), bounds.fTop * scale.height(),
      SkScalarCeilToInt(SkScalarAbs(scale.width() * bounds.width())),
      SkScalarCeilToInt(SkScalarAbs(scale.height() * bounds.height())));
}

sk_sp<SkPicture> PaintRecord::ToSkPicture(
    const SkRect& bounds,
    ImageProvider* image_provider,
    const PlaybackCallbacks& callbacks) const {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds);
  PlaybackParams params(image_provider, SkM44(), callbacks);
  Playback(canvas, params);
  return recorder.finishRecordingAsPicture();
}

PaintRecord::const_iterator PaintRecord::begin() const {
  return const_iterator(buffer());
}

PaintRecord::const_iterator PaintRecord::end() const {
  return const_iterator(buffer()).end();
}

}  // namespace cc
