// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_recorder.h"

#include "cc/paint/display_item_list.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

PaintRecorder::PaintRecorder() {
  display_item_list_ = base::MakeRefCounted<DisplayItemList>(
      DisplayItemList::kToBeReleasedAsPaintOpBuffer);
}
PaintRecorder::~PaintRecorder() = default;

PaintCanvas* PaintRecorder::beginRecording(const SkRect& bounds) {
  display_item_list_->StartPaint();
  canvas_ = CreateCanvas(display_item_list_.get(), bounds);
  return getRecordingCanvas();
}

sk_sp<PaintRecord> PaintRecorder::finishRecordingAsPicture() {
  // SkPictureRecorder users expect that their saves are automatically
  // closed for them.
  //
  // NOTE: Blink paint in general doesn't appear to need this, but the
  // RecordingImageBufferSurface::fallBackToRasterCanvas finishing off the
  // current frame depends on this.  Maybe we could remove this assumption and
  // just have callers do it.
  canvas_->restoreToCount(1);

  // Some users (e.g. printing) use the existence of the recording canvas
  // to know if recording is finished, so reset it here.
  canvas_.reset();

  // The rect doesn't matter, since we just release the record.
  display_item_list_->EndPaintOfUnpaired(gfx::Rect());
  display_item_list_->Finalize();
  return display_item_list_->ReleaseAsRecord();
}

std::unique_ptr<RecordPaintCanvas> PaintRecorder::CreateCanvas(
    DisplayItemList* list,
    const SkRect& bounds) {
  return std::make_unique<RecordPaintCanvas>(list, bounds);
}

bool PaintRecorder::ListHasDrawOps() const {
  return display_item_list_->has_draw_ops();
}

size_t PaintRecorder::num_paint_ops() const {
  return display_item_list_->num_paint_ops();
}

}  // namespace cc
