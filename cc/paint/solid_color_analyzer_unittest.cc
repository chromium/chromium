// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/paint/solid_color_analyzer.h"

#include <optional>

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/record_paint_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {

class SolidColorAnalyzerTest : public testing::Test {
 protected:
  void TearDown() override { Reset(); }
  void Reset() { canvas_.ReleaseAsRecord(); }

  bool IsSolidColor(int max_ops_to_analyze = 1,
                    const gfx::Rect& rect = gfx::Rect(0, 0, 100, 100)) {
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(
        canvas_.ReleaseAsRecord().buffer(), rect, max_ops_to_analyze, nullptr);
    return !!color;
  }

  SkColor4f GetColor(int max_ops_to_analyze = 1,
                     const gfx::Rect rect = gfx::Rect(0, 0, 100, 100)) {
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(
        canvas_.ReleaseAsRecord().buffer(), rect, max_ops_to_analyze, nullptr);
    EXPECT_TRUE(color);
    return color ? *color : SkColors::kTransparent;
  }

  RecordPaintCanvas canvas_;

 private:
  std::optional<SolidColorAnalyzer> analyzer_;
};

TEST_F(SolidColorAnalyzerTest, Empty) {
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTransparent) {
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(0, 12, 34, 56));
  canvas_.clear(color);
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearSolid) {
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 65, 43, 21));
  canvas_.clear(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTranslucent) {
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(128, 11, 22, 33));
  canvas_.clear(color);
#if BUILDFLAG(IS_MAC)
  // TODO(andrescj): remove the special treatment of OS_MAC once
  // https://crbug.com/922899 is fixed.
  EXPECT_FALSE(IsSolidColor());
#else
  EXPECT_EQ(color, GetColor());
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(SolidColorAnalyzerTest, DrawColor) {
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  canvas_.drawColor(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawOval) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  canvas_.drawOval(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRect) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.clipRect(rect, SkClipOp::kIntersect, false);
  canvas_.drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

// TODO(vmpstr): Generalize the DrawRect test cases so that we can test both
// Rect and RRect.
TEST_F(SolidColorAnalyzerTest, DrawRRect) {
  SkRect rect = SkRect::MakeWH(200, 200);
  SkRRect rrect;
  rrect.setRectXY(rect, 5, 5);
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  canvas_.drawRRect(rrect, flags);
  EXPECT_EQ(color, GetColor(1, gfx::Rect(5, 5, 190, 190)));
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipped) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.clipRect(SkRect::MakeWH(50, 50), SkClipOp::kIntersect, false);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClippedDifference) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect drawRect = SkRect::MakeWH(200, 200);
  canvas_.clipRect(drawRect, SkClipOp::kIntersect, false);
  SkRect differenceRect = SkRect::MakeXYWH(50, 50, 200, 200);
  // Using difference should always make this fail.
  canvas_.clipRect(differenceRect, SkClipOp::kDifference, false);
  canvas_.drawRect(drawRect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateNotSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas_.translate(1, 1);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(101, 101);
  canvas_.translate(1, 1);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, TwoOpsNotSolid) {
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 65, 43, 21));
  canvas_.clear(color);
  canvas_.clear(color);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeClear) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kClear);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.drawRect(rect, flags);
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeSrcOver) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kSrcOver);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectRotated) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.rotate(50);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledNotSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.scale(0.1f, 0.1f);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(10, 10);
  canvas_.scale(10, 10);
  canvas_.drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectFilterPaint) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setImageFilter(sk_make_sp<OffsetPaintFilter>(10, 10, nullptr));
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipPath) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);

  SkPath path;
  path.moveTo(0, 0);
  path.lineTo(128, 50);
  path.lineTo(255, 0);
  path.lineTo(255, 255);
  path.lineTo(0, 255);

  SkRect rect = SkRect::MakeWH(200, 200);
  canvas_.clipPath(path, SkClipOp::kIntersect);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectTranslucent) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(128, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas_.drawRect(rect, flags);
#if BUILDFLAG(IS_MAC)
  // TODO(andrescj): remove the special treatment of OS_MAC once
  // https://crbug.com/922899 is fixed.
  EXPECT_FALSE(IsSolidColor());
#else
  EXPECT_EQ(color, GetColor());
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(SolidColorAnalyzerTest, DrawRectTranslucentOverNonSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 50);
  canvas_.drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(128, 0, 128, 0));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas_.drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor(2 /* max_ops_to_analyze */));
}

TEST_F(SolidColorAnalyzerTest, DrawRectOpaqueOccludesNonSolid) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 50);
  canvas_.drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(255, 0, 128, 0));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas_.drawRect(rect, flags);
  EXPECT_EQ(color, GetColor(2 /* max_ops_to_analyze */));
}

TEST_F(SolidColorAnalyzerTest, DrawRectSolidWithSrcOverBlending) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(64, 40, 50, 60));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas_.drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(128, 10, 20, 30));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas_.drawRect(rect, flags);
#if BUILDFLAG(IS_MAC)
  // TODO(andrescj): remove the special treatment of OS_MAC once
  // https://crbug.com/922899 is fixed.
  EXPECT_FALSE(IsSolidColor());
#else
  // Float precision makes testing for equality a pain, so we'll just use ints
  EXPECT_EQ(SkColorSetARGB(160, 16, 26, 36),
            GetColor(2 /* max_ops_to_analyze */).toSkColor());
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(SolidColorAnalyzerTest, SaveLayer) {
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  canvas_.saveLayer(SkRect::MakeWH(200, 200), flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, ClipRRectCoversCanvas) {
  SkVector radii[4] = {
      SkVector::Make(10.0, 15.0), SkVector::Make(20.0, 25.0),
      SkVector::Make(30.0, 35.0), SkVector::Make(40.0, 45.0),
  };

  SkVector radii_scale[4] = {
      SkVector::Make(100.0, 150.0), SkVector::Make(200.0, 250.0),
      SkVector::Make(300.0, 350.0), SkVector::Make(400.0, 450.0),
  };

  int rr_size = 600;
  int canvas_size = 255;
  gfx::Rect canvas_rect(canvas_size, canvas_size);
  PaintFlags flags;
  flags.setColor(SkColors::kWhite);

  struct {
    SkVector offset;
    SkVector offset_scale;
    bool expected;
  } cases[] = {
      // Not within bounding box of |rr|.
      {SkVector::Make(100, 100), SkVector::Make(100, 100), false},

      // Intersects UL corner.
      {SkVector::Make(0, 0), SkVector::Make(0, 0), false},

      // Between UL and UR.
      {SkVector::Make(-50, 0), SkVector::Make(-50, -15), true},

      // Intersects UR corner.
      {SkVector::Make(canvas_size - rr_size, 0),
       SkVector::Make(canvas_size - rr_size, 0), false},

      // Between UR and LR.
      {SkVector::Make(canvas_size - rr_size, -50), SkVector::Make(-305, -80),
       true},

      // Intersects LR corner.
      {SkVector::Make(canvas_size - rr_size, canvas_size - rr_size),
       SkVector::Make(canvas_size - rr_size, canvas_size - rr_size), false},

      // Between LL and LR
      {SkVector::Make(-50, canvas_size - rr_size), SkVector::Make(-205, -310),
       true},

      // Intersects LL corner
      {SkVector::Make(0, canvas_size - rr_size),
       SkVector::Make(0, canvas_size - rr_size), false},

      // Between UL and LL
      {SkVector::Make(0, -50), SkVector::Make(-15, -60), true},

      // In center
      {SkVector::Make(-100, -100), SkVector::Make(-100, -100), true},
  };

  for (int case_scale = 0; case_scale < 2; ++case_scale) {
    bool scaled = case_scale > 0;
    for (size_t i = 0; i < std::size(cases); ++i) {
      Reset();

      SkRect bounding_rect = SkRect::MakeXYWH(
          scaled ? cases[i].offset_scale.x() : cases[i].offset.x(),
          scaled ? cases[i].offset_scale.y() : cases[i].offset.y(), rr_size,
          rr_size);

      SkRRect rr;
      rr.setRectRadii(bounding_rect, scaled ? radii_scale : radii);

      canvas_.clipRRect(rr, SkClipOp::kIntersect, false);
      canvas_.drawRect(RectToSkRect(canvas_rect), flags);
      EXPECT_EQ(cases[i].expected, IsSolidColor(1, canvas_rect))
          << "Case " << i << ", " << scaled << " failed.";
    }
  }
}

}  // namespace
}  // namespace cc
