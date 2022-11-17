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
#include "ui/gfx/geometry/size.h"

namespace cc {

class DisplayItemList;

class CC_PAINT_EXPORT PaintRecorderBase {
 public:
  PaintRecorderBase(const PaintRecorderBase&) = delete;
  PaintRecorderBase& operator=(const PaintRecorderBase&) = delete;

  sk_sp<PaintRecord> finishRecordingAsPicture();

  bool ListHasDrawOps() const;

  // Ops with nested paint ops are considered as a single op.
  size_t num_paint_ops() const;

  size_t TotalOpCount() const { return display_item_list_->TotalOpCount(); }
  size_t OpBytesUsed() const { return display_item_list_->OpBytesUsed(); }

  // Only valid while recording.
  PaintCanvas* getRecordingCanvas() {
    return is_recording_ ? canvas_.get() : nullptr;
  }

 protected:
  PaintRecorderBase();
  ~PaintRecorderBase();

  // The subclass must create `canvas_` before calling this method.
  void beginRecording();

  bool is_recording_ = false;
  scoped_refptr<DisplayItemList> display_item_list_;
  std::unique_ptr<RecordPaintCanvas> canvas_;
};

class CC_PAINT_EXPORT PaintRecorder : public PaintRecorderBase {
 public:
  // Begins recording. The returned PaintCanvas doesn't support inspection of
  // the current clip and the CTM during recording.
  PaintCanvas* beginRecording();
};

class CC_PAINT_EXPORT InspectablePaintRecorder : public PaintRecorderBase {
 public:
  // Begins recording. The returned PaintCanvas supports inspection of the
  // current clip and the CTM during recording. `size` doesn't affect the
  // recorded results because all operations will be recorded regardless of it,
  // but it determines the top-level device clip.
  PaintCanvas* beginRecording(const gfx::Size& size);

 private:
  gfx::Size size_;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_RECORDER_H_
