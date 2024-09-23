// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/test_skcanvas.h"

#include "third_party/skia/include/gpu/ganesh/gl/GrGLInterface.h"

namespace cc {
SaveCountingCanvas::SaveCountingCanvas() : SkNoDrawCanvas(100, 100) {}

SkCanvas::SaveLayerStrategy SaveCountingCanvas::getSaveLayerStrategy(
    const SaveLayerRec& rec) {
  save_count_++;
  return SkNoDrawCanvas::getSaveLayerStrategy(rec);
}

void SaveCountingCanvas::willRestore() {
  restore_count_++;
}

void SaveCountingCanvas::onDrawRect(const SkRect& rect, const SkPaint& paint) {
  draw_rect_ = rect;
  paint_ = paint;
}

void SaveCountingCanvas::onDrawPaint(const SkPaint& paint) {
  paint_ = paint;
}

MockCanvas::MockCanvas() : SkNoDrawCanvas(100, 100) {
  context_ = GrDirectContext::MakeMock(nullptr);
}

MockCanvas::~MockCanvas() = default;

}  // namespace cc
