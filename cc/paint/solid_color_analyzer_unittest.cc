// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/solid_color_analyzer.h"

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "cc/paint/display_item_list.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/record_paint_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {
namespace {

class SolidColorAnalyzerTest : public testing::Test {
 public:
  void SetUp() override {
    display_item_list_ = base::MakeRefCounted<DisplayItemList>(
        DisplayItemList::kToBeReleasedAsPaintOpBuffer);
    display_item_list_->StartPaint();
  }

  void TearDown() override {
    Finalize();
    canvas_.reset();
    display_item_list_ = nullptr;
    buffer_ = nullptr;
  }

  void Reset() {
    TearDown();
    SetUp();
  }

  void Initialize(const gfx::Rect& rect = gfx::Rect(0, 0, 100, 100)) {
    canvas_.emplace(display_item_list_.get(), gfx::RectToSkRect(rect));
    rect_ = rect;
  }
  RecordPaintCanvas* canvas() { return &*canvas_; }

  bool IsSolidColor(int max_ops_to_analyze = 1) {
    Finalize();
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(
        buffer_.get(), rect_, max_ops_to_analyze, nullptr);
    return !!color;
  }

  SkColor4f GetColor(int max_ops_to_analyze = 1) {
    Finalize();
    auto color = SolidColorAnalyzer::DetermineIfSolidColor(
        buffer_.get(), rect_, max_ops_to_analyze, nullptr);
    EXPECT_TRUE(color);
    return color ? *color : SkColors::kTransparent;
  }

 private:
  void Finalize() {
    if (buffer_)
      return;
    display_item_list_->EndPaintOfUnpaired(gfx::Rect());
    display_item_list_->Finalize();
    buffer_ = display_item_list_->ReleaseAsRecord();
  }

  gfx::Rect rect_;
  scoped_refptr<DisplayItemList> display_item_list_;
  sk_sp<PaintOpBuffer> buffer_;
  absl::optional<RecordPaintCanvas> canvas_;
  absl::optional<SolidColorAnalyzer> analyzer_;
};

TEST_F(SolidColorAnalyzerTest, Empty) {
  Initialize();
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTransparent) {
  Initialize();
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(0, 12, 34, 56));
  canvas()->clear(color);
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearSolid) {
  Initialize();
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 65, 43, 21));
  canvas()->clear(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, ClearTranslucent) {
  Initialize();
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(128, 11, 22, 33));
  canvas()->clear(color);
#if BUILDFLAG(IS_MAC)
  // TODO(andrescj): remove the special treatment of OS_MAC once
  // https://crbug.com/922899 is fixed.
  EXPECT_FALSE(IsSolidColor());
#else
  EXPECT_EQ(color, GetColor());
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(SolidColorAnalyzerTest, DrawColor) {
  Initialize();
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  canvas()->drawColor(color);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawOval) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  canvas()->drawOval(SkRect::MakeWH(100, 100), flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRect) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->clipRect(rect, SkClipOp::kIntersect, false);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

// TODO(vmpstr): Generalize the DrawRect test cases so that we can test both
// Rect and RRect.
TEST_F(SolidColorAnalyzerTest, DrawRRect) {
  SkRect rect = SkRect::MakeWH(200, 200);
  SkRRect rrect;
  rrect.setRectXY(rect, 5, 5);
  gfx::Rect canvas_rect(5, 5, 190, 190);
  Initialize(canvas_rect);
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  canvas()->drawRRect(rrect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipped) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->clipRect(SkRect::MakeWH(50, 50), SkClipOp::kIntersect, false);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClippedDifference) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect drawRect = SkRect::MakeWH(200, 200);
  canvas()->clipRect(drawRect, SkClipOp::kIntersect, false);
  SkRect differenceRect = SkRect::MakeXYWH(50, 50, 200, 200);
  // Using difference should always make this fail.
  canvas()->clipRect(differenceRect, SkClipOp::kDifference, false);
  canvas()->drawRect(drawRect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateNotSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas()->translate(1, 1);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectWithTranslateSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(101, 101);
  canvas()->translate(1, 1);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, TwoOpsNotSolid) {
  Initialize();
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 65, 43, 21));
  canvas()->clear(color);
  canvas()->clear(color);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeClear) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kClear);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(SkColors::kTransparent, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectBlendModeSrcOver) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setBlendMode(SkBlendMode::kSrcOver);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectRotated) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->rotate(50);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledNotSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->scale(0.1f, 0.1f);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectScaledSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(10, 10);
  canvas()->scale(10, 10);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectFilterPaint) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);
  flags.setImageFilter(sk_make_sp<OffsetPaintFilter>(10, 10, nullptr));
  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectClipPath) {
  Initialize();
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
  canvas()->clipPath(path, SkClipOp::kIntersect);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor());
}

TEST_F(SolidColorAnalyzerTest, DrawRectTranslucent) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(128, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas()->drawRect(rect, flags);
#if BUILDFLAG(IS_MAC)
  // TODO(andrescj): remove the special treatment of OS_MAC once
  // https://crbug.com/922899 is fixed.
  EXPECT_FALSE(IsSolidColor());
#else
  EXPECT_EQ(color, GetColor());
#endif  // BUILDFLAG(IS_MAC)
}

TEST_F(SolidColorAnalyzerTest, DrawRectTranslucentOverNonSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 50);
  canvas()->drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(128, 0, 128, 0));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas()->drawRect(rect, flags);
  EXPECT_FALSE(IsSolidColor(2 /* max_ops_to_analyze */));
}

TEST_F(SolidColorAnalyzerTest, DrawRectOpaqueOccludesNonSolid) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 128, 0, 0));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 50);
  canvas()->drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(255, 0, 128, 0));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas()->drawRect(rect, flags);
  EXPECT_EQ(color, GetColor(2 /* max_ops_to_analyze */));
}

TEST_F(SolidColorAnalyzerTest, DrawRectSolidWithSrcOverBlending) {
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(64, 40, 50, 60));
  flags.setColor(color);
  SkRect rect = SkRect::MakeWH(100, 100);
  canvas()->drawRect(rect, flags);
  color = SkColor4f::FromColor(SkColorSetARGB(128, 10, 20, 30));
  flags.setColor(color);
  rect = SkRect::MakeWH(100, 100);
  canvas()->drawRect(rect, flags);
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
  Initialize();
  PaintFlags flags;
  SkColor4f color = SkColor4f::FromColor(SkColorSetARGB(255, 11, 22, 33));
  flags.setColor(color);

  SkRect rect = SkRect::MakeWH(200, 200);
  canvas()->saveLayer(&rect, &flags);
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
      Initialize(canvas_rect);

      SkRect bounding_rect = SkRect::MakeXYWH(
          scaled ? cases[i].offset_scale.x() : cases[i].offset.x(),
          scaled ? cases[i].offset_scale.y() : cases[i].offset.y(), rr_size,
          rr_size);

      SkRRect rr;
      rr.setRectRadii(bounding_rect, scaled ? radii_scale : radii);

      canvas()->clipRRect(rr, SkClipOp::kIntersect, false);
      canvas()->drawRect(RectToSkRect(canvas_rect), flags);
      EXPECT_EQ(cases[i].expected, IsSolidColor())
          << "Case " << i << ", " << scaled << " failed.";
    }
  }
}

}  // namespace
}  // namespace cc
