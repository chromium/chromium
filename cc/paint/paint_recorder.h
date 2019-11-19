// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORDER_H_
#define CC_PAINT_PAINT_RECORDER_H_

#include "base/compiler_specific.h"
#include "base/optional.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"

namespace cc {

class DisplayItemList;

class CC_PAINT_EXPORT PaintRecorder {
 public:
  PaintRecorder();
  PaintRecorder(const PaintRecorder&) = delete;
  ~PaintRecorder();

  PaintRecorder& operator=(const PaintRecorder&) = delete;

  PaintCanvas* beginRecording(const SkRect& bounds);

  // TODO(enne): should make everything go through the non-rect version.
  // See comments in RecordPaintCanvas ctor for why.
  PaintCanvas* beginRecording(SkScalar width, SkScalar height) {
    return beginRecording(SkRect::MakeWH(width, height));
  }

  // Only valid while recording.
  ALWAYS_INLINE RecordPaintCanvas* getRecordingCanvas() {
    return canvas_.has_value() ? &canvas_.value() : nullptr;
  }

  sk_sp<PaintRecord> finishRecordingAsPicture();

 private:
  scoped_refptr<DisplayItemList> display_item_list_;
  base::Optional<RecordPaintCanvas> canvas_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORDER_H_
