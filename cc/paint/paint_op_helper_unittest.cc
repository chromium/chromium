// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/paint_op_helper.h"

#include <array>

#include "cc/paint/paint_canvas.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/skia_common.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkTextBlob.h"
#include "third_party/skia/include/core/SkTileMode.h"
#include "third_party/skia/include/effects/SkLumaColorFilter.h"
#include "third_party/skia/include/private/chromium/Slug.h"

namespace cc {
namespace {

TEST(PaintOpHelper, PaintRecordEmptyToString) {
  PaintOpBuffer buffer;
  EXPECT_EQ(PaintOpHelper::ToString(buffer.ReleaseAsRecord()),
            "<PaintRecord>[]");
}

TEST(PaintOpHelper, PaintRecordOneOpToString) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  EXPECT_EQ(PaintOpHelper::ToString(buffer.ReleaseAsRecord()),
            "<PaintRecord>[SaveOp()]");
}

TEST(PaintOpHelper, PaintRecordMultipleOpsToString) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  buffer.push<RotateOp>(360.0f);
  EXPECT_EQ(PaintOpHelper::ToString(buffer.ReleaseAsRecord()),
            "<PaintRecord>[SaveOp(), RotateOp(degrees=360.000)]");
}

TEST(PaintOpHelper, AnnotateToString) {
  AnnotateOp op(PaintCanvas::AnnotationType::kUrl, SkRect::MakeXYWH(1, 2, 3, 4),
                nullptr);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "AnnotateOp(type=URL, rect=[1.000,2.000 3.000x4.000], data=(nil))");
}

TEST(PaintOpHelper, ClipPathToString) {
  ClipPathOp op(SkPath(), SkClipOp::kDifference, true,
                UsePaintCache::kDisabled);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "ClipPathOp(path=<SkPath>, op=kDifference, antialias=true, "
            "use_cache=false)");
}

TEST(PaintOpHelper, ClipRectToString) {
  ClipRectOp op(SkRect::MakeXYWH(10.1f, 20.2f, 30.3f, 40.4f),
                SkClipOp::kIntersect, false);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "ClipRectOp(rect=[10.100,20.200 30.300x40.400], op=kIntersect, "
            "antialias=false)");
}

TEST(PaintOpHelper, ClipRRectToString) {
  ClipRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(1, 2, 3, 4)),
                 SkClipOp::kDifference, false);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "ClipRRectOp(rrect=[bounded by 1.000,2.000 3.000x4.000], "
            "op=kDifference, antialias=false)");
}

TEST(PaintOpHelper, ConcatToString) {
  ConcatOp op(SkM44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "ConcatOp(matrix=[  1.0000   2.0000   3.0000   4.0000][  5.0000   "
            "6.0000   7.0000   8.0000][  9.0000  10.0000  11.0000  12.0000][ "
            "13.0000  14.0000  15.0000  16.0000]])");
}

TEST(PaintOpHelper, DrawColorToString) {
  DrawColorOp op({0.1, 0.2, 0.3, 0.4}, SkBlendMode::kSrc);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "DrawColorOp(color=rgba(0.100000, 0.200000, 0.300000, 0.400000), "
            "mode=kSrc)");
}

TEST(PaintOpHelper, DrawDRRectToString) {
  DrawDRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(1, 2, 3, 4)),
                  SkRRect::MakeRect(SkRect::MakeXYWH(5, 6, 7, 8)),
                  PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawDRRectOp(outer=[bounded by 1.000,2.000 3.000x4.000], inner=[bounded "
      "by 5.000,6.000 7.000x8.000], flags=[color=rgba(0, 0, 0, 255), "
      "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawImageToString) {
  DrawImageOp op(PaintImage(), 10.5f, 20.3f);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawImageOp(image=<paint image>, left=10.500, top=20.300, "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawImageRectToString) {
  DrawImageRectOp op(PaintImage(), SkRect::MakeXYWH(1, 2, 3, 4),
                     SkRect::MakeXYWH(5, 6, 7, 8),
                     SkCanvas::kStrict_SrcRectConstraint);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawImageRectOp(image=<paint image>, src=[1.000,2.000 3.000x4.000], "
      "dst=[5.000,6.000 7.000x8.000], constraint=kStrict_SrcRectConstraint, "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawIRectToString) {
  DrawIRectOp op(SkIRect::MakeXYWH(1, 2, 3, 4), PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "DrawIRectOp(rect=[1,2 3x4], flags=[color=rgba(0, 0, 0, 255), "
            "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
            "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
            "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
            "colorFilter=(nil), "
            "shader=(nil), hasShader=false, shaderIsOpaque=false, "
            "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
            "supportsFoldingAlpha=true, isValid=true, "
            "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawLineToString) {
  DrawLineOp op(1.1f, 2.2f, 3.3f, 4.4f, PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawLineOp(x0=1.100, y0=2.200, x1=3.300, y1=4.400, flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawOvalToString) {
  DrawOvalOp op(SkRect::MakeXYWH(100, 200, 300, 400), PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawOvalOp(oval=[100.000,200.000 300.000x400.000], flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawPathToString) {
  SkPath path;
  DrawPathOp op(path, PaintFlags(), UsePaintCache::kDisabled);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "DrawPathOp(path=<SkPath>, flags=[color=rgba(0, 0, 0, 255), "
            "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
            "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
            "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
            "colorFilter=(nil), "
            "shader=(nil), hasShader=false, shaderIsOpaque=false, "
            "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
            "supportsFoldingAlpha=true, isValid=true, "
            "hasDiscardableImages=false], use_cache=false)");
}

TEST(PaintOpHelper, DrawRecordToString) {
  DrawRecordOp op((PaintRecord()));
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "DrawRecordOp(record=<PaintRecord>[])");
}

TEST(PaintOpHelper, DrawRectToString) {
  DrawRectOp op(SkRect::MakeXYWH(-1, -2, -3, -4), PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawRectOp(rect=[-1.000,-2.000 -3.000x-4.000], flags=[color=rgba(0, 0, "
      "0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawRRectToString) {
  DrawRRectOp op(SkRRect::MakeRect(SkRect::MakeXYWH(-1, -2, 3, 4)),
                 PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawRRectOp(rrect=[bounded by -1.000,-2.000 3.000x4.000], "
      "flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, "
      "isDither=false, filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawSlugToString) {
  DrawSlugOp op(nullptr, PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawSlugOp(flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, "
      "isAntiAlias=false, isDither=false, filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawTextBlobToString) {
  DrawTextBlobOp op(nullptr, 100, -222, PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "DrawTextBlobOp(blob=(nil), x=100.000, y=-222.000, flags=[color=rgba(0, "
      "0, 0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), "
      "hasShader=false, shaderIsOpaque=false, pathEffect=(nil), "
      "imageFilter=(nil), drawLooper=(nil), supportsFoldingAlpha=true, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, DrawVerticesToString) {
  auto verts = base::MakeRefCounted<RefCountedBuffer<SkPoint>>(
      std::vector<SkPoint>{{100, 100}});
  auto uvs = base::MakeRefCounted<RefCountedBuffer<SkPoint>>(
      std::vector<SkPoint>{{1, 1}});
  auto indices = base::MakeRefCounted<RefCountedBuffer<uint16_t>>(
      std::vector<uint16_t>{0, 0, 0});

  DrawVerticesOp op(verts, uvs, indices, PaintFlags());
  EXPECT_EQ(
      PaintOpHelper::ToString(op),
      "DrawVerticesOp(flags=[color=rgba(0, 0, 0, 255), blendMode=kSrcOver, "
      "isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
      "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
      "colorFilter=(nil), shader=(nil), hasShader=false, "
      "shaderIsOpaque=false, pathEffect=(nil), imageFilter=(nil), "
      "drawLooper=(nil), supportsFoldingAlpha=true, isValid=true, "
      "hasDiscardableImages=false])");
}

TEST(PaintOpHelper, NoopToString) {
  NoopOp op;
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "NoopOp()");
}

TEST(PaintOpHelper, RestoreToString) {
  RestoreOp op;
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "RestoreOp()");
}

TEST(PaintOpHelper, RotateToString) {
  RotateOp op(360);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "RotateOp(degrees=360.000)");
}

TEST(PaintOpHelper, SaveToString) {
  SaveOp op;
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "SaveOp()");
}

TEST(PaintOpHelper, SaveLayerToString) {
  SaveLayerOp op(SkRect::MakeXYWH(1, 2, 3, 4), PaintFlags());
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(
      str,
      "SaveLayerOp(bounds=[1.000,2.000 3.000x4.000], flags=[color=rgba(0, 0, "
      "0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=(nil), drawLooper=(nil), "
      "supportsFoldingAlpha=true, isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, SaveLayerWithFilterToString) {
  SkRect bounds = SkRect::MakeXYWH(1, 2, 3, 4);
  PaintFlags flags;
  flags.setImageFilter(sk_make_sp<DropShadowPaintFilter>(
      0.0f, 0.0f, 0.0f, 0.0f, SkColors::kTransparent,
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground, nullptr));
  SaveLayerOp op(bounds, flags);
  EXPECT_EQ(
      PaintOpHelper::ToString(op),
      "SaveLayerOp(bounds=[1.000,2.000 3.000x4.000], flags=[color=rgba(0, 0, "
      "0, 255), blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
      "filterQuality=kNone_SkFilterQuality, "
      "strokeWidth=0.000, strokeMiter=4.000, strokeCap=kButt_Cap, "
      "strokeJoin=kMiter_Join, colorFilter=(nil), "
      "shader=(nil), hasShader=false, shaderIsOpaque=false, "
      "pathEffect=(nil), imageFilter=DropShadowPaintFilter(dx=0.000, dy=0.000, "
      "sigma_x=0.000, sigma_y=0.000, color=rgba(0.000000, 0.000000, 0.000000, "
      "0.000000), shadow_mode=kDrawShadowAndForeground, input=(nil), "
      "crop_rect=(nil)), drawLooper=(nil), supportsFoldingAlpha=false, "
      "isValid=true, hasDiscardableImages=false])");
}

TEST(PaintOpHelper, SaveLayerAlphaToString) {
  SaveLayerAlphaOp op(SkRect::MakeXYWH(1, 2, 3, 4), 1.0f);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "SaveLayerAlphaOp(bounds=[1.000,2.000 3.000x4.000], alpha=1.000)");
}

TEST(PaintOpHelper, SaveLayerFiltersToString) {
  PaintFlags flags;
  SaveLayerFiltersOp op(
      std::array<sk_sp<PaintFilter>, 2>{
          sk_make_sp<BlurPaintFilter>(1.0f, 2.0f, SkTileMode::kRepeat,
                                      /*input=*/nullptr),
          nullptr},
      flags);
  EXPECT_EQ(PaintOpHelper::ToString(op),
            "SaveLayerFiltersOp(flags=[color=rgba(0, 0, 0, 255), "
            "blendMode=kSrcOver, isAntiAlias=false, isDither=false, "
            "filterQuality=kNone_SkFilterQuality, strokeWidth=0.000, "
            "strokeMiter=4.000, strokeCap=kButt_Cap, strokeJoin=kMiter_Join, "
            "colorFilter=(nil), shader=(nil), hasShader=false, "
            "shaderIsOpaque=false, pathEffect=(nil), imageFilter=(nil), "
            "drawLooper=(nil), supportsFoldingAlpha=true, isValid=true, "
            "hasDiscardableImages=false], "
            "filters={BlurPaintFilter(sigma_x=1.000, sigma_y=2.000, "
            "tile_mode=kRepeat, input=(nil), crop_rect=(nil)), (nil)})");
}

TEST(PaintOpHelper, ScaleToString) {
  ScaleOp op(12, 13.9f);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "ScaleOp(sx=12.000, sy=13.900)");
}

TEST(PaintOpHelper, SetMatrixToString) {
  SetMatrixOp op(SkM44(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16));
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str,
            "SetMatrixOp(matrix=[  1.0000   2.0000   3.0000   4.0000][  5.0000 "
            "  6.0000   7.0000   8.0000][  9.0000  10.0000  11.0000  12.0000][ "
            "13.0000  14.0000  15.0000  16.0000]])");
}

TEST(PaintOpHelper, TranslateToString) {
  TranslateOp op(0, 0);
  std::string str = PaintOpHelper::ToString(op);
  EXPECT_EQ(str, "TranslateOp(dx=0.000, dy=0.000)");
}

TEST(PaintOpHelperFilters, ColorFilterPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  ColorFilterPaintFilter filter(ColorFilter::MakeLuma(),
                                /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "ColorFilterPaintFilter(color_filter=ColorFilter, input=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, BlurPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  BlurPaintFilter filter(1.f, 2.f, SkTileMode::kRepeat,
                         /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "BlurPaintFilter(sigma_x=1.000, sigma_y=2.000, tile_mode=kRepeat, "
            "input=(nil), crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, DropShadowPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  DropShadowPaintFilter filter(
      1.f, 2.f, 3.f, 4.f, SkColors::kWhite,
      DropShadowPaintFilter::ShadowMode::kDrawShadowOnly,
      /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "DropShadowPaintFilter(dx=1.000, dy=2.000, sigma_x=3.000, "
            "sigma_y=4.000, color=rgba(1.000000, 1.000000, 1.000000, "
            "1.000000), shadow_mode=kDrawShadowOnly, input=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, MagnifierPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  MagnifierPaintFilter filter(SkRect::MakeWH(100.f, 100.f), /*zoom_amount=*/2.f,
                              /*inset=*/0.1f, /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "MagnifierPaintFilter(lens_bounds=[0.000,0.000 100.000x100.000], "
            "zoom_amount=2.000, inset=0.100, input=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, ComposePaintFilter) {
  ComposePaintFilter filter(sk_make_sp<OffsetPaintFilter>(
                                /*dx=*/0.1f, /*dy=*/0.2f, /*input=*/nullptr),
                            nullptr);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "ComposePaintFilter(outer=OffsetPaintFilter(dx=0.1, dy=0.2, "
            "input=(nil), crop_rect=(nil)), inner=(nil), crop_rect=(nil))");
}

TEST(PaintOpHelperFilters, AlphaThresholdPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  AlphaThresholdPaintFilter filter(SkRegion(SkIRect::MakeWH(100, 100)),
                                   /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "AlphaThresholdPaintFilter(region=[0,0 100x100], "
            "input=(nil), crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, XfermodePaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  XfermodePaintFilter filter(SkBlendMode::kSrc,
                             /*background=*/nullptr, /*foreground=*/nullptr,
                             &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "XfermodePaintFilter(blend_mode=kSrc, "
            "background=(nil), foreground=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, ArithmeticPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  ArithmeticPaintFilter filter(/*k1=*/0.1f, /*k2=*/0.2f, /*k3=*/0.3f,
                               /*k4=*/0.4f, /*enforce_pm_color=*/true,
                               /*background=*/nullptr, /*foreground=*/nullptr,
                               &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "ArithmeticPaintFilter(k1=0.1, k2=0.2, k3=0.3, k4=0.4, "
            "enfore_pm_color=true, background=(nil), foreground=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, MatrixConvolutionPaintFilter) {
  SkScalar scalars[9] = {1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f};
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  MatrixConvolutionPaintFilter filter(
      /*kernel_size=*/SkISize::Make(3, 3),
      /*kernel=*/scalars, /*gain=*/0.1f, /*bias=*/0.2f,
      /*kernel_offset=*/SkIPoint::Make(2, 2), SkTileMode::kRepeat,
      /*convolve_alpha=*/false,
      /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "MatrixConvolutionPaintFilter(kernel_size=SkISize(3, 3), "
            "kernel=[1.000, 2.000, 3.000, 4.000, 5.000, 6.000, 7.000, 8.000, "
            "9.000], gain=0.100, bias=0.200, kernel_offset=SkIPoint(2, 2), "
            "tile_mode=kRepeat, convolve_alpha=false, input=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, DisplacementMapEffectPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  DisplacementMapEffectPaintFilter filter(
      SkColorChannel::kR, SkColorChannel::kR, /*scale=*/0.1f,
      /*displacement=*/nullptr, /*color=*/nullptr, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "DisplacementMapEffectPaintFilter(channel_x=kR, channel_y=kR, "
            "scale=0.100, displacement=(nil), color=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, ImagePaintFilter) {
  ImagePaintFilter filter(CreateDiscardablePaintImage(gfx::Size(100, 100)),
                          SkRect::MakeWH(100.f, 100.f),
                          SkRect::MakeWH(100.f, 100.f),
                          PaintFlags::FilterQuality::kNone);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "ImagePaintFilter(image=<paint image>, "
            "src_rect=[0.000,0.000 100.000x100.000], "
            "dst_rect=[0.000,0.000 100.000x100.000], "
            "filter_quality=kNone_SkFilterQuality, crop_rect=(nil))");
}

TEST(PaintOpHelperFilters, RecordPaintFilter) {
  PaintOpBuffer buffer;
  buffer.push<SaveOp>();
  RecordPaintFilter filter(buffer.ReleaseAsRecord(),
                           SkRect::MakeWH(100.f, 100.f),
                           /*raster_scale=*/{0.5f, 0.8f},
                           RecordPaintFilter::ScalingBehavior::kFixedScale);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "RecordPaintFilter(record=<PaintRecord>[SaveOp()], "
            "record_bounds=[0.000,0.000 100.000x100.000], "
            "raster_scale=[0.5x0.8], scaling_behavior=kFixedScale, "
            "crop_rect=(nil))");
}

TEST(PaintOpHelperFilters, MergePaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  sk_sp<PaintFilter> filters[] = {
      sk_make_sp<ImagePaintFilter>(
          CreateDiscardablePaintImage(gfx::Size(100, 100)),
          SkRect::MakeWH(100.f, 100.f), SkRect::MakeWH(100.f, 100.f),
          PaintFlags::FilterQuality::kNone),
      nullptr};
  MergePaintFilter filter(filters, 2, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "MergePaintFilter(input_count=2, input=[ImagePaintFilter("
            "image=<paint image>, "
            "src_rect=[0.000,0.000 100.000x100.000], "
            "dst_rect=[0.000,0.000 100.000x100.000], "
            "filter_quality=kNone_SkFilterQuality, crop_rect=(nil)), (nil)], "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, MorphologyPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  MorphologyPaintFilter filter(MorphologyPaintFilter::MorphType::kErode,
                               /*radius_x=*/1, /*radius_y=*/2, nullptr,
                               &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "MorphologyPaintFilter(morph_type=kErode, radius_x=1, radius_y=2, "
            "input=(nil), crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, OffsetPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  OffsetPaintFilter filter(/*dx=*/0.1f, /*dy=*/0.2f, /*input=*/nullptr,
                           &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "OffsetPaintFilter(dx=0.1, dy=0.2, input=(nil), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, TilePaintFilter) {
  TilePaintFilter filter(SkRect::MakeWH(100.f, 100.f),
                         SkRect::MakeWH(200.f, 200.f),
                         /*input=*/nullptr);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "TilePaintFilter(src=[0.000,0.000 100.000x100.000], "
            "dst=[0.000,0.000 200.000x200.000], input=(nil), crop_rect=(nil))");
}

TEST(PaintOpHelperFilters, TurbulencePaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  const auto tile_size = SkISize::Make(3, 4);
  TurbulencePaintFilter filter(
      TurbulencePaintFilter::TurbulenceType::kFractalNoise,
      /*base_frequency_x=*/0.1f, /*base_frequency_y=*/0.2f, /*num_octaves=*/2,
      /*seed=*/0.3f, /*tile_size=*/&tile_size, &crop_rect);
  EXPECT_EQ(PaintOpHelper::ToString(filter),
            "TurbulencePaintFilter(turbulence_type=kFractalNoise, "
            "base_frequency_x=0.100, base_frequency_y=0.200, num_octaves=2, "
            "seed=0.300, tile_size=SkISize(3, 4), "
            "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, ShaderPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  ShaderPaintFilter filter(
      PaintShader::MakeImage(CreateDiscardablePaintImage(gfx::Size(100, 100)),
                             /*tx=*/SkTileMode::kClamp,
                             /*ty=*/SkTileMode::kRepeat,
                             /*local_matrix=*/nullptr),
      /*alpha=*/1.0f, PaintFlags::FilterQuality::kMedium,
      SkImageFilters::Dither::kYes, &crop_rect);
  EXPECT_EQ(
      PaintOpHelper::ToString(filter),
      "ShaderPaintFilter(shader=[type=kImage, flags=0, end_radius=0, "
      "start_radius=0, tx=0, ty=1, fallback_color=rgba(0.000000, 0.000000, "
      "0.000000, 0.000000), scaling_behavior=kRasterAtScale, "
      "local_matrix=(nil), center=[0.000,0.000], tile=[0.000,0.000 "
      "0.000x0.000], start_point=[0.000,0.000], end_point=[0.000,0.000], "
      "start_degrees=0, end_degrees=0, image=<paint image>, record=(nil), "
      "id=4294967295, tile_scale=(nil), colors=(nil), positions=(nil)], "
      "alpha=1.000, filter_quality=kMedium_SkFilterQuality, dither=kYes, "
      "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, MatrixPaintFilter) {
  MatrixPaintFilter filter(SkMatrix::I(), PaintFlags::FilterQuality::kHigh,
                           /*input=*/nullptr);
  EXPECT_EQ(
      PaintOpHelper::ToString(filter),
      "MatrixPaintFilter(matrix=[  1.0000   0.0000   0.0000][  0.0000   1.0000 "
      "  0.0000][  0.0000   0.0000   1.0000]], "
      "filter_quality=kHigh_SkFilterQuality, input=(nil), crop_rect=(nil))");
}

TEST(PaintOpHelperFilters, LightingDistantPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  LightingDistantPaintFilter filter(
      PaintFilter::LightingType::kSpecular,
      /*direction=*/SkPoint3::Make(0.1f, 0.2f, 0.3f), SkColors::kWhite,
      /*surface_scale=*/0.1f, /*kconstant=*/0.2f,
      /*shininess=*/0.3f, /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(
      PaintOpHelper::ToString(filter),
      "LightingDistantPaintFilter(lighting_type=kSpecular, "
      "direction=SkPoint3(0.100, 0.200, 0.300), "
      "light_color=rgba(1.000000, 1.000000, 1.000000, 1.000000), "
      "surface_scale=0.100, kconstant=0.200, shininess=0.300, input=(nil), "
      "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, LightingPointPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  LightingPointPaintFilter filter(
      PaintFilter::LightingType::kSpecular,
      /*location=*/SkPoint3::Make(0.1f, 0.2f, 0.3f), SkColors::kWhite,
      /*surface_scale=*/0.1f, /*kconstant=*/0.2f,
      /*shininess=*/0.3f, /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(
      PaintOpHelper::ToString(filter),
      "LightingPointPaintFilter(lighting_type=kSpecular, "
      "location=SkPoint3(0.100, 0.200, 0.300), "
      "light_color=rgba(1.000000, 1.000000, 1.000000, 1.000000), "
      "surface_scale=0.100, kconstant=0.200, shininess=0.300, input=(nil), "
      "crop_rect=[0.000,0.000 100.000x100.000])");
}

TEST(PaintOpHelperFilters, LightingSpotPaintFilter) {
  PaintFilter::CropRect crop_rect(SkRect::MakeWH(100.f, 100.f));
  LightingSpotPaintFilter filter(
      PaintFilter::LightingType::kSpecular,
      /*location=*/SkPoint3::Make(0.1f, 0.2f, 0.3f),
      /*target=*/SkPoint3::Make(0.4f, 0.5f, 0.6f), /*specular_exponent=*/0.1f,
      /*cutoff_angle=*/0.2f, SkColors::kWhite,
      /*surface_scale=*/0.1f, /*kconstant=*/0.2f,
      /*shininess=*/0.3f, /*input=*/nullptr, &crop_rect);
  EXPECT_EQ(
      PaintOpHelper::ToString(filter),
      "LightingSpotPaintFilter(lighting_type=kSpecular, "
      "location=SkPoint3(0.100, 0.200, 0.300), "
      "target=SkPoint3(0.400, 0.500, 0.600), "
      "specular_exponent=0.100, cutoff_angle=0.200, "
      "light_color=rgba(1.000000, 1.000000, 1.000000, 1.000000), "
      "surface_scale=0.100, kconstant=0.200, shininess=0.300, input=(nil), "
      "crop_rect=[0.000,0.000 100.000x100.000])");
}

}  // namespace
}  // namespace cc
