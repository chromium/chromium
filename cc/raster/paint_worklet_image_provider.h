// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
#define CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_

#include "cc/cc_export.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_worklet_input.h"

namespace cc {

// PaintWorkletImageProvider is a storage class for PaintWorklets and their
// painted content for use during rasterisation.
//
// PaintWorklet-based images are not painted at Blink Paint time; instead a
// placeholder PaintWorkletInput is put in place and the painting is done later
// from the cc-impl thread. By the time raster happens the resultant PaintRecord
// is available, and this class provides the lookup from input to record.
class CC_EXPORT PaintWorkletImageProvider {
 public:
  explicit PaintWorkletImageProvider(PaintWorkletRecordMap records);
  PaintWorkletImageProvider(const PaintWorkletImageProvider&) = delete;
  PaintWorkletImageProvider(PaintWorkletImageProvider&& other);
  ~PaintWorkletImageProvider();

  PaintWorkletImageProvider& operator=(const PaintWorkletImageProvider&) =
      delete;
  PaintWorkletImageProvider& operator=(PaintWorkletImageProvider&& other);

  ImageProvider::ScopedResult GetPaintRecordResult(
      scoped_refptr<PaintWorkletInput> input);

 private:
  PaintWorkletRecordMap records_;
};

}  // namespace cc

#endif  // CC_RASTER_PAINT_WORKLET_IMAGE_PROVIDER_H_
