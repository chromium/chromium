// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_recorder.h"

#include "cc/paint/display_item_list.h"

namespace cc {

PaintRecorderBase::PaintRecorderBase()
    : display_item_list_(base::MakeRefCounted<DisplayItemList>(
          DisplayItemList::kToBeReleasedAsPaintOpBuffer)) {}

PaintRecorderBase::~PaintRecorderBase() = default;

void PaintRecorderBase::beginRecording() {
  DCHECK(!is_recording_);
  is_recording_ = true;

  // The subclass must create canvas_ before calling this method.
  DCHECK(canvas_);
  display_item_list_->StartPaint();
}

sk_sp<PaintRecord> PaintRecorderBase::finishRecordingAsPicture() {
  DCHECK(canvas_);
  DCHECK(is_recording_);
  is_recording_ = false;

  // Some users expect that their saves are automatically closed for them.
  // Maybe we could remove this assumption and just have callers do it.
  // canvas_ is not reset in case it can be reused for the next recording.
  canvas_->restoreToCount(1);

  if (canvas_->NeedsFlush())
    canvas_.reset();

  // The rect doesn't matter, since we just release the record.
  display_item_list_->EndPaintOfUnpaired(gfx::Rect());
  return display_item_list_->FinalizeAndReleaseAsRecord();
}

bool PaintRecorderBase::ListHasDrawOps() const {
  return display_item_list_->has_draw_ops();
}

size_t PaintRecorderBase::num_paint_ops() const {
  return display_item_list_->num_paint_ops();
}

PaintCanvas* PaintRecorder::beginRecording() {
  if (!canvas_)
    canvas_ = std::make_unique<RecordPaintCanvas>(display_item_list_.get());
  PaintRecorderBase::beginRecording();
  return canvas_.get();
}

PaintCanvas* InspectablePaintRecorder::beginRecording(const gfx::Size& size) {
  if (!canvas_ || size != size_) {
    canvas_ = std::make_unique<InspectableRecordPaintCanvas>(
        display_item_list_.get(), size);
  }
  size_ = size;
  PaintRecorderBase::beginRecording();
  return canvas_.get();
}

}  // namespace cc
