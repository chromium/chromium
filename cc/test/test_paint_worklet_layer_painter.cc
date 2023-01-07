// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_paint_worklet_layer_painter.h"

#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"

namespace cc {

TestPaintWorkletLayerPainter::TestPaintWorkletLayerPainter() = default;

TestPaintWorkletLayerPainter::~TestPaintWorkletLayerPainter() = default;

void TestPaintWorkletLayerPainter::DispatchWorklets(
    PaintWorkletJobMap,
    DoneCallback done_callback) {
  // To enforce good behavior: the new callback should not be null, and the
  // saved one should be consumed (via |TakeDoneCallback|) before
  // |DispatchWorklets| is called again.
  DCHECK(!done_callback.is_null());
  DCHECK(done_callback_.is_null());

  done_callback_ = std::move(done_callback);
}

bool TestPaintWorkletLayerPainter::HasOngoingDispatch() const {
  return !done_callback_.is_null();
}

}  // namespace cc
