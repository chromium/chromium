// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_raster_flags.h"

#include <utility>
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "cc/paint/paint_op.h"
#include "cc/paint/paint_shader.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkTileMode.h"

namespace cc {
namespace {
class MockImageProvider : public ImageProvider {
 public:
  MockImageProvider() = default;
  ~MockImageProvider() override { EXPECT_EQ(ref_count_, 0); }

  ScopedResult GetRasterContent(const DrawImage& draw_image) override {
    DCHECK(!draw_image.paint_image().IsPaintWorklet());
    ref_count_++;

    SkBitmap bitmap;
    bitmap.allocN32Pixels(10, 10);
    sk_sp<SkImage> image = SkImages::RasterFromBitmap(bitmap);

    return ScopedResult(
        DecodedDrawImage(image, nullptr, SkSize::MakeEmpty(),
                         SkSize::Make(1.0f, 1.0f), draw_image.filter_quality(),
                         true),
        base::BindOnce(&MockImageProvider::UnrefImage, base::Unretained(this)));
  }

  void UnrefImage() {
    ref_count_--;
    CHECK_GE(ref_count_, 0);
  }

  int ref_count() const { return ref_count_; }

 private:
  int ref_count_ = 0;
};

class MockPaintWorkletImageProvider : public ImageProvider {
 public:
  MockPaintWorkletImageProvider() = default;
  ~MockPaintWorkletImageProvider() override = default;

  ScopedResult GetRasterContent(const DrawImage& draw_image) override {
    return ScopedResult(PaintRecord());
  }
};
}  // namespace

TEST(ScopedRasterFlagsTest, DecodePaintWorkletImageShader) {
  float width = 100;
  float height = 100;
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(gfx::SizeF(width, height));
  auto image = CreatePaintWorkletPaintImage(input);
  SkMatrix pattern_matrix;
  SkRect tile_rect = SkRect::MakeXYWH(0, 0, width, height);
  auto shader =
      PaintShader::MakeImage(image, SkTileMode::kRepeat, SkTileMode::kRepeat,
                             &pattern_matrix, &tile_rect);
  PaintFlags flags;
  flags.setShader(shader);

  MockPaintWorkletImageProvider provider;
  ScopedRasterFlags scoped_flags(&flags, &provider, SkMatrix::I(), 0, 1.0f);
  ASSERT_TRUE(scoped_flags.flags());
  EXPECT_TRUE(scoped_flags.flags()->getShader()->shader_type() ==
              PaintShader::Type::kPaintRecord);
}

TEST(ScopedRasterFlagsTest, KeepsDecodesAlive) {
  PaintOpBuffer buffer;
  buffer.push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                           0.f);
  buffer.push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                           0.f);
  buffer.push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                           0.f);
  auto record_shader = PaintShader::MakePaintRecord(
      buffer.ReleaseAsRecord(), SkRect::MakeWH(100, 100), SkTileMode::kClamp,
      SkTileMode::kClamp, &SkMatrix::I());
  record_shader->set_has_animated_images(true);

  MockImageProvider provider;
  PaintFlags flags;
  flags.setShader(record_shader);
  {
    ScopedRasterFlags scoped_flags(&flags, &provider, SkMatrix::I(), 0, 1.0f);
    ASSERT_TRUE(scoped_flags.flags());
    EXPECT_NE(scoped_flags.flags(), &flags);
    SkPaint paint = scoped_flags.flags()->ToSkPaint();
    ASSERT_TRUE(paint.getShader());
    EXPECT_EQ(provider.ref_count(), 3);
  }
  EXPECT_EQ(provider.ref_count(), 0);
}

TEST(ScopedRasterFlagsTest, NoImageProvider) {
  PaintFlags flags;
  flags.setAlphaf(1.0f);
  flags.setShader(PaintShader::MakeImage(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), SkTileMode::kClamp,
      SkTileMode::kClamp, &SkMatrix::I()));
  ScopedRasterFlags scoped_flags(&flags, nullptr, SkMatrix::I(), 0, 0.1f);
  EXPECT_NE(scoped_flags.flags(), &flags);
  EXPECT_EQ(scoped_flags.flags()->getAlphaf(), 1.0f * 0.1f);
}

TEST(ScopedRasterFlagsTest, ThinAliasedStroke) {
  PaintFlags flags;
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setAntiAlias(false);

  struct {
    SkMatrix ctm;
    float alpha;

    bool expect_same_flags;
    bool expect_aa;
    float expect_stroke_width;
    float expect_alpha;
  } tests[] = {
      // No downscaling                    => no stroke change.
      {SkMatrix::Scale(1.0f, 1.0f), 1.0f, true, false, 1.0f, 1.0f},
      // Symmetric downscaling             => modulated hairline stroke.
      {SkMatrix::Scale(0.5f, 0.5f), 1.0f, false, false, 0.0f, 0.5f},
      // Symmetric downscaling w/ alpha    => modulated hairline stroke.
      {SkMatrix::Scale(0.5f, 0.5f), 0.5f, false, false, 0.0f, 0.25f},
      // Anisotropic scaling              => AA stroke.
      {SkMatrix::Scale(0.5f, 1.5f), 1.0f, false, true, 1.0f, 1.0f},
  };

  for (const auto& test : tests) {
    ScopedRasterFlags scoped_flags(&flags, nullptr, test.ctm, 0, test.alpha);
    ASSERT_TRUE(scoped_flags.flags());

    EXPECT_EQ(scoped_flags.flags() == &flags, test.expect_same_flags);
    EXPECT_EQ(scoped_flags.flags()->isAntiAlias(), test.expect_aa);
    EXPECT_EQ(scoped_flags.flags()->getStrokeWidth(), test.expect_stroke_width);
    EXPECT_LE(std::abs(scoped_flags.flags()->getAlphaf() - test.expect_alpha),
              0.01f);
  }
}

}  // namespace cc
