// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_RECORDER_H_
#define CC_PAINT_PAINT_RECORDER_H_

#include <memory>
#include "base/compiler_specific.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/record_paint_canvas.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

class CC_PAINT_EXPORT PaintRecorder {
 public:
  // Begins recording. The returned PaintCanvas doesn't support inspection of
  // the current clip and the CTM during recording.
  PaintCanvas* beginRecording();

  PaintRecord finishRecordingAsPicture();

  // Only valid while recording.
  PaintCanvas* getRecordingCanvas() {
    return is_recording_ ? &canvas_ : nullptr;
  }

 private:
  bool is_recording_ = false;
  RecordPaintCanvas canvas_;
};

class CC_PAINT_EXPORT InspectablePaintRecorder {
 public:
  InspectablePaintRecorder();
  ~InspectablePaintRecorder();

  // Begins recording. The returned PaintCanvas supports inspection of the
  // current clip and the CTM during recording. `size` doesn't affect the
  // recorded results because all operations will be recorded regardless of it,
  // but it determines the top-level device clip.
  PaintCanvas* beginRecording(const gfx::Size& size);

  PaintRecord finishRecordingAsPicture();

  // Only valid while recording.
  PaintCanvas* getRecordingCanvas() const {
    DCHECK(!is_recording_ || canvas_);
    return is_recording_ ? canvas_.get() : nullptr;
  }

 private:
  bool is_recording_ = false;
  std::unique_ptr<InspectableRecordPaintCanvas> canvas_;
  gfx::Size size_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORDER_H_
