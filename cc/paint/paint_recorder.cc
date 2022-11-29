// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_recorder.h"

#include <utility>

namespace cc {

PaintCanvas* PaintRecorder::beginRecording() {
  DCHECK(!is_recording_);
  is_recording_ = true;
  return &canvas_;
}

sk_sp<PaintRecord> PaintRecorder::finishRecordingAsPicture() {
  DCHECK(is_recording_);
  is_recording_ = false;
  return canvas_.ReleaseAsRecord();
}

InspectablePaintRecorder::InspectablePaintRecorder() = default;
InspectablePaintRecorder::~InspectablePaintRecorder() = default;

PaintCanvas* InspectablePaintRecorder::beginRecording(const gfx::Size& size) {
  DCHECK(!is_recording_);
  is_recording_ = true;

  if (!canvas_ || size != size_) {
    canvas_ = std::make_unique<InspectableRecordPaintCanvas>(size);
  }
  size_ = size;
  return canvas_.get();
}

sk_sp<PaintRecord> InspectablePaintRecorder::finishRecordingAsPicture() {
  DCHECK(canvas_);
  DCHECK(is_recording_);
  is_recording_ = false;
  return canvas_->ReleaseAsRecord();
}

}  // namespace cc
