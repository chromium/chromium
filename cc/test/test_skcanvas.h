// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_SKCANVAS_H_
#define CC_TEST_TEST_SKCANVAS_H_

#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/gpu/ganesh/GrDirectContext.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"

namespace cc {

class SaveCountingCanvas : public SkNoDrawCanvas {
 public:
  SaveCountingCanvas();

  // Note: getSaveLayerStrategy is used as "willSave", as willSave
  // is not always called.
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override;
  void willRestore() override;
  void onDrawRect(const SkRect& rect, const SkPaint& paint) override;
  void onDrawPaint(const SkPaint& paint) override;

  int save_count_ = 0;
  int restore_count_ = 0;
  SkRect draw_rect_;
  SkPaint paint_;
};

class MockCanvas : public SkNoDrawCanvas {
 public:
  MockCanvas();
  ~MockCanvas() override;

  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    OnSaveLayer();
    return SkNoDrawCanvas::getSaveLayerStrategy(rec);
  }

  void onDrawPaint(const SkPaint& paint) override {
    OnDrawPaintWithColor(paint.getColor());
  }
  void onDrawRect(const SkRect& rect, const SkPaint& paint) override {
    OnDrawRectWithColor(paint.getColor());
  }
  GrRecordingContext* recordingContext() const override {
    return context_.get();
  }

  MOCK_METHOD1(OnDrawPaintWithColor, void(SkColor));
  MOCK_METHOD1(OnDrawRectWithColor, void(SkColor));
  MOCK_METHOD0(OnSaveLayer, void());
  MOCK_METHOD0(willRestore, void());
  MOCK_METHOD0(willSave, void());
  MOCK_METHOD2(onDrawPath, void(const SkPath&, const SkPaint&));
  MOCK_METHOD5(onDrawImage2,
               void(const SkImage*,
                    SkScalar,
                    SkScalar,
                    const SkSamplingOptions&,
                    const SkPaint*));
  MOCK_METHOD6(onDrawImageRect2,
               void(const SkImage*,
                    const SkRect&,
                    const SkRect&,
                    const SkSamplingOptions&,
                    const SkPaint*,
                    SrcRectConstraint));
  MOCK_METHOD5(onDrawArc,
               void(const SkRect&, SkScalar, SkScalar, bool, const SkPaint&));
  MOCK_METHOD1(didConcat44, void(const SkM44&));
  MOCK_METHOD1(didSetM44, void(const SkM44&));
  MOCK_METHOD2(didScale, void(SkScalar, SkScalar));
  MOCK_METHOD2(didTranslate, void(SkScalar, SkScalar));
  MOCK_METHOD2(onDrawOval, void(const SkRect&, const SkPaint&));
  MOCK_METHOD2(onCustomCallback, void(SkCanvas*, uint32_t));
  MOCK_METHOD2(onDrawGlyphRunList,
               void(const sktext::GlyphRunList&, const SkPaint&));

  sk_sp<GrDirectContext> context_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_SKCANVAS_H_
