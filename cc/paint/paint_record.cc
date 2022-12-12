// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_record.h"

#include <utility>

#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

sk_sp<SkPicture> ToSkPicture(
    sk_sp<const PaintRecord> record,
    const SkRect& bounds,
    ImageProvider* image_provider,
    PlaybackParams::CustomDataRasterCallback custom_callback,
    PlaybackParams::ConvertOpCallback convert_op_callback) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds);
  PlaybackParams params(image_provider);
  params.custom_callback = std::move(custom_callback);
  params.convert_op_callback = std::move(convert_op_callback);
  record->Playback(canvas, params);
  return recorder.finishRecordingAsPicture();
}

}  // namespace cc
