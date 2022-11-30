// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/paint_op_helper.h"
#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_op_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkTextBlob.h"

namespace cc {
namespace {

TEST(PaintOpHelper, AnnotateToString) {
  AnnotateOp op(PaintCanvas::AnnotationType::URL, SkRect::MakeXYWH(1, 2, 3, 4),
                nullptr);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "AnnotateOp(type=URL, rect=[1.000,2.000 3.000x4.000], data=(nil))");
}

TEST(PaintOpHelper, ClipPathToString) {
  ClipPathOp op(SkPath(), SkClipOp::kDifference, true,
                UsePaintCache::kDisabled);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "ClipPathOp(path=<SkPath>, op=kDifference, antialias=true, "
            "use_cache=false)");
}

TEST(PaintOpHelper, ClipRectToString) {
  ClipRectOp op(SkRect::MakeXYWH(10.1f, 20.2f, 30.3f, 40.4f),
                SkClipOp::kIntersect, false);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "ClipRectOp(rect=[10.100,20.200 30.300x40.400], op=kIntersect, "
            "antialias=false)");
}

TEST(PaintOpHelper, ClipRRectToString) {
  ClipRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(1, 2, 3, 4)),
                 SkClipOp::kDifference, false);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "ClipRRectOp(rrect=[bounded by 1.000,2.000 3.000x4.000], "
            "op=kDifference, antialias=false)");
}

TEST(PaintOpHelper, ConcatToString) {
  ConcatOp op(SkM44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "ConcatOp(matrix=[  1.0000   2.0000   3.0000   4.0000][  5.0000   "
            "6.0000   7.0000   8.0000][  9.0000  10.0000  11.0000  12.0000][ "
            "13.0000  14.0000  15.0000  16.0000]])");
}

TEST(PaintOpHelper, DrawColorToString) {
  DrawColorOp op({0.1, 0.2, 0.3, 0.4}, SkBlendMode::kSrc);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "DrawColorOp(color=rgba(0.100000, 0.200000, 0.300000, 0.400000), "
            "mode=kSrc)");
}

TEST(PaintOpHelper, DrawDRRectToString) {
  DrawDRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(1, 2, 3, 4)),
                  SkRRect::MakeRect(SkRect::MakeXYWH(5, 6, 7, 8)),
                  PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawDRRectOp(outer=[bounded by 1.000,2.000 3.000x4.000], inner=[bounded "
      "by 5.000,6.000 7.000x8.000], flags=[color=rgba(0, 0, 0, 255), "
      "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "maskFilter=(nil), shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "isSimpleOpacity=true, supportsFoldingAlpha=true, isValid=true, "
      "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawImageToString) {
  DrawImageOp op(PaintImage(), 10.5f, 20.3f);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawImageOp(image=<paint image>, left=10.500, top=20.300, "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawImageRectToString) {
  DrawImageRectOp op(PaintImage(), SkRect::MakeXYWH(1, 2, 3, 4),
                     SkRect::MakeXYWH(5, 6, 7, 8),
                     SkCanvas::kStrict_SrcRectConstraint);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawImageRectOp(image=<paint image>, src=[1.000,2.000 3.000x4.000], "
      "dst=[5.000,6.000 7.000x8.000], constraint=kStrict_SrcRectConstraint, "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawIRectToString) {
  DrawIRectOp op(SkIRect::MakeXYWH(1, 2, 3, 4), PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "DrawIRectOp(rect=[1,2 3x4], flags=[color=rgba(0, 0, 0, 255), "
            "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
            "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
            "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
            "colorFilter=(nil), maskFilter=(nil), "
            "shader=(nil), hasShader=false, shaderIsOpaque=false, "
            "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
            "isSimpleOpacity=true, supportsFoldingAlpha=true, isValid=true, "
            "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawLineToString) {
  DrawLineOp op(1.1f, 2.2f, 3.3f, 4.4f, PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawLineOp(x0=1.100, y0=2.200, x1=3.300, y1=4.400, flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawOvalToString) {
  DrawOvalOp op(SkRect::MakeXYWH(100, 200, 300, 400), PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawOvalOp(oval=[100.000,200.000 300.000x400.000], flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawPathToString) {
  SkPath path;
  DrawPathOp op(path, PaintFlags(), UsePaintCache::kDisabled);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "DrawPathOp(path=<SkPath>, flags=[color=rgba(0, 0, 0, 255), "
            "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
            "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
            "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
            "colorFilter=(nil), maskFilter=(nil), "
            "shader=(nil), hasShader=false, shaderIsOpaque=false, "
            "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
            "isSimpleOpacity=true, supportsFoldingAlpha=true, isValid=true, "
            "hasDiscardableImages=false], use_cache=false)");
}

TEST(PaintOpHelper, DrawRecordToString) {
  DrawRecordOp op(nullptr);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "DrawRecordOp(record=(nil))");
}

TEST(PaintOpHelper, DrawRectToString) {
  DrawRectOp op(SkRect::MakeXYWH(-1, -2, -3, -4), PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawRectOp(rect=[-1.000,-2.000 -3.000x-4.000], flags=[color=rgba(0, 0, "
      "0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "maskFilter=(nil), shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "isSimpleOpacity=true, supportsFoldingAlpha=true, isValid=true, "
      "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawRRectToString) {
  DrawRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(-1, -2, 3, 4)),
                 PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawRRectOp(rrect=[bounded by -1.000,-2.000 3.000x4.000], "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawTextBlobToString) {
  DrawTextBlobOp op(nullptr, 100, -222, PaintFlags());
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "DrawTextBlobOp(blob=(nil), x=100.000, y=-222.000, flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), maskFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), isSimpleOpacity=true, "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, NoopToString) {
  NoopOp op;
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "NoopOp()");
}

TEST(PaintOpHelper, RestoreToString) {
  RestoreOp op;
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "RestoreOp()");
}

TEST(PaintOpHelper, RotateToString) {
  RotateOp op(360);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "RotateOp(degrees=360.000)");
}

TEST(PaintOpHelper, SaveToString) {
  SaveOp op;
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "SaveOp()");
}

TEST(PaintOpHelper, SaveLayerToString) {
  SkRect bounds = SkRect::MakeXYWH(1, 2, 3, 4);
  SaveLayerOp op(&bounds, nullptr);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(
      str,
      "SaveLayerOp(bounds=[1.000,2.000 3.000x4.000], flags=[color=rgba(0, 0, "
      "0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "maskFilter=(nil), shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "isSimpleOpacity=true, supportsFoldingAlpha=true, isValid=true, "
      "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, SaveLayerAlphaToString) {
  SkRect bounds = SkRect::MakeXYWH(1, 2, 3, 4);
  SaveLayerAlphaOp op(&bounds, 1.0f);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "SaveLayerAlphaOp(bounds=[1.000,2.000 3.000x4.000], alpha=255)");
}

TEST(PaintOpHelper, ScaleToString) {
  ScaleOp op(12, 13.9f);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "ScaleOp(sx=12.000, sy=13.900)");
}

TEST(PaintOpHelper, SetMatrixToString) {
  SetMatrixOp op(SkM44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str,
            "SetMatrixOp(matrix=[  1.0000   2.0000   3.0000   4.0000][  5.0000 "
            "  6.0000   7.0000   8.0000][  9.0000  10.0000  11.0000  12.0000][ "
            "13.0000  14.0000  15.0000  16.0000]])");
}

TEST(PaintOpHelper, TranslateToString) {
  TranslateOp op(0, 0);
  std::string str = PaintOpHelper::ToString(&op);
  EXPECT_EQ(str, "TranslateOp(dx=0.000, dy=0.000)");
}

}  // namespace
}  // namespace cc
