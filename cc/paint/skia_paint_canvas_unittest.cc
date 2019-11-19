// Copyright 2018 The Chromium Authors. All rights reserved.
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
  EXPECT_CALL(mock_canvas, onFlush()).Times(2);
  EXPECT_CALL(mock_canvas, OnDrawRectWithColor(_)).Times(11);

  SkiaPaintCanvas::ContextFlushes context_flushes;
  context_flushes.enable = true;
  context_flushes.max_draws_before_flush = 4;
  SkiaPaintCanvas paint_canvas(&mock_canvas, nullptr, context_flushes);
  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  for (int i = 0; i < 11; i++)
    paint_canvas.drawRect(rect, flags);
}

TEST(SkiaPaintCanvasTest, ContextFlushesRecording) {
  StrictMock<MockCanvas> mock_canvas;
  EXPECT_CALL(mock_canvas, onFlush()).Times(2);
  EXPECT_CALL(mock_canvas, OnDrawRectWithColor(_)).Times(11);

  PaintRecorder recorder;
  SkRect rect = SkRect::MakeWH(10, 10);
  PaintFlags flags;
  recorder.beginRecording(rect);
  for (int i = 0; i < 11; i++)
    recorder.getRecordingCanvas()->drawRect(rect, flags);
  auto record = recorder.finishRecordingAsPicture();

  SkiaPaintCanvas::ContextFlushes context_flushes;
  context_flushes.enable = true;
  context_flushes.max_draws_before_flush = 4;
  SkiaPaintCanvas paint_canvas(&mock_canvas, nullptr, context_flushes);
  paint_canvas.drawPicture(record);
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
