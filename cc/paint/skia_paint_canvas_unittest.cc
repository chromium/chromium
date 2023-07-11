// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skia_paint_canvas.h"

#include "cc/paint/paint_recorder.h"
#include "cc/test/test_skcanvas.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;
using ::testing::Return;

namespace cc {

TEST(SkiaPaintCanvasTest, ContextFlushesDirect) {
  StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnDrawRectWithColor(_)).Times(11);

  constexpr int kFlushBatch = 4;  // flush every 5 draws
  SkiaPaintCanvas::ContextFlushes context_flushes;
  context_flushes.enable = true;
  context_flushes.max_draws_before_flush = kFlushBatch;
  SkiaPaintCanvas paint_canvas(&mock_canvas, nullptr, context_flushes);
  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  int expected_pending[11] = {1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1};
  for (int i : expected_pending) {
    paint_canvas.drawRect(rect, flags);
    EXPECT_EQ(paint_canvas.pendingOps(), i);
  }
}

TEST(SkiaPaintCanvasTest, ContextFlushesRecording) {
  StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnDrawRectWithColor(_)).Times(11);

  PaintRecorder recorder;
  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  recorder.beginRecording();
  for (int i = 0; i < 11; i++)
    recorder.getRecordingCanvas()->drawRect(rect, flags);
  auto record = recorder.finishRecordingAsPicture();

  SkiaPaintCanvas::ContextFlushes context_flushes;
  context_flushes.enable = true;
  context_flushes.max_draws_before_flush = 3;  // flush every 4 draws
  SkiaPaintCanvas paint_canvas(&mock_canvas, nullptr, context_flushes);
  paint_canvas.drawPicture(record);
  // There should be 3 leftover ops (11 % 4) and we should have flushed twice.
  EXPECT_EQ(paint_canvas.pendingOps(), 3);
}

TEST(SkiaPaintCanvasTest, ContextFlushesDisabled) {
  StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, OnDrawRectWithColor(_)).Times(11);

  SkiaPaintCanvas::ContextFlushes context_flushes;
  SkiaPaintCanvas paint_canvas(&mock_canvas, nullptr, context_flushes);
  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  for (int i = 0; i < 11; i++)
    paint_canvas.drawRect(rect, flags);
}

}  // namespace cc
