// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORDER_H_
#define CC_PAINT_PAINT_RECORDER_H_

#include <memory>
#include "base/compiler_specific.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"

namespace cc {

class DisplayItemList;

class CC_PAINT_EXPORT PaintRecorder {
 public:
  PaintRecorder();
  PaintRecorder(const PaintRecorder&) = delete;
  virtual ~PaintRecorder();

  PaintRecorder& operator=(const PaintRecorder&) = delete;

  PaintCanvas* beginRecording(const SkRect& bounds);

  // TODO(enne): should make everything go through the non-rect version.
  // See comments in RecordPaintCanvas ctor for why.
  PaintCanvas* beginRecording(SkScalar width, SkScalar height) {
    return beginRecording(SkRect::MakeWH(width, height));
  }

  // Only valid while recording.
  ALWAYS_INLINE RecordPaintCanvas* getRecordingCanvas() {
    return canvas_.get();
  }

  sk_sp<PaintRecord> finishRecordingAsPicture();

  bool ListHasDrawOps() const;

  // Ops with nested paint ops are considered as a single op.
  size_t num_paint_ops() const;

  size_t TotalOpCount() const { return display_item_list_->TotalOpCount(); }
  size_t OpBytesUsed() const { return display_item_list_->OpBytesUsed(); }

 protected:
  virtual std::unique_ptr<RecordPaintCanvas> CreateCanvas(DisplayItemList* list,
                                                          const SkRect& bounds);

 private:
  scoped_refptr<DisplayItemList> display_item_list_;
  std::unique_ptr<RecordPaintCanvas> canvas_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORDER_H_
