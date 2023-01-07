// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_PAINT_WORKLET_LAYER_PAINTER_H_
#define CC_PAINT_PAINT_WORKLET_LAYER_PAINTER_H_

#include "base/functional/callback.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_record.h"
#include "cc/paint/paint_worklet_job.h"

namespace cc {

// PaintWorkletLayerPainter bridges between the compositor and the PaintWorklet
// thread, providing hooks for the compositor to paint PaintWorklet content that
// Blink has deferred on.
class CC_PAINT_EXPORT PaintWorkletLayerPainter {
 public:
  virtual ~PaintWorkletLayerPainter() {}

  // Asynchronously paints a set of PaintWorklet instances. The results are
  // returned via the provided callback, on the same thread that originally
  // called this method.
  //
  // Only one dispatch is allowed at a time; the calling code should not call
  // |DispatchWorklets| again until the passed |DoneCallback| has been called.
  using DoneCallback = base::OnceCallback<void(PaintWorkletJobMap)>;
  virtual void DispatchWorklets(PaintWorkletJobMap, DoneCallback) = 0;

  // Returns whether or not a dispatched set of PaintWorklet instances is
  // currently being painted.
  virtual bool HasOngoingDispatch() const = 0;
};

}  // namespace cc

#endif  // CC_PAINT_PAINT_WORKLET_LAYER_PAINTER_H_
