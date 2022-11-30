// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_record.h"

#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace cc {

sk_sp<SkPicture> ToSkPicture(
    sk_sp<PaintRecord> record,
    const SkRect& bounds,
    ImageProvider* image_provider,
    PlaybackParams::CustomDataRasterCallback callback) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds);
  PlaybackParams params(image_provider);
  params.custom_callback = callback;
  record->Playback(canvas, params);
  return recorder.finishRecordingAsPicture();
}

sk_sp<const SkPicture> ToSkPicture(
    sk_sp<const PaintRecord> record,
    const SkRect& bounds,
    ImageProvider* image_provider,
    PlaybackParams::CustomDataRasterCallback callback) {
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(bounds);
  PlaybackParams params(image_provider);
  params.custom_callback = callback;
  record->Playback(canvas, params);
  return recorder.finishRecordingAsPicture();
}

}  // namespace cc
