// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_raster_flags.h"

#include "base/bind.h"
#include "base/callback.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/paint/paint_shader.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    sk_sp<SkImage> image = SkImage::MakeFromBitmap(bitmap);

    return ScopedResult(
        DecodedDrawImage(image, SkSize::MakeEmpty(), SkSize::Make(1.0f, 1.0f),
                         draw_image.filter_quality(), true),
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
    auto record = sk_make_sp<PaintOpBuffer>();
    return ScopedResult(std::move(record));
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
  ScopedRasterFlags scoped_flags(&flags, &provider, SkMatrix::I(), 0, 255);
  ASSERT_TRUE(scoped_flags.flags());
  EXPECT_TRUE(scoped_flags.flags()->getShader()->shader_type() ==
              PaintShader::Type::kPaintRecord);
}

TEST(ScopedRasterFlagsTest, KeepsDecodesAlive) {
  auto record = sk_make_sp<PaintOpBuffer>();
  record->push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                            0.f, nullptr);
  record->push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                            0.f, nullptr);
  record->push<DrawImageOp>(CreateDiscardablePaintImage(gfx::Size(10, 10)), 0.f,
                            0.f, nullptr);
  auto record_shader = PaintShader::MakePaintRecord(
      record, SkRect::MakeWH(100, 100), SkTileMode::kClamp, SkTileMode::kClamp,
      &SkMatrix::I());
  record_shader->set_has_animated_images(true);

  MockImageProvider provider;
  PaintFlags flags;
  flags.setShader(record_shader);
  {
    ScopedRasterFlags scoped_flags(&flags, &provider, SkMatrix::I(), 0, 255);
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
  flags.setAlpha(255);
  flags.setShader(PaintShader::MakeImage(
      CreateDiscardablePaintImage(gfx::Size(10, 10)), SkTileMode::kClamp,
      SkTileMode::kClamp, &SkMatrix::I()));
  ScopedRasterFlags scoped_flags(&flags, nullptr, SkMatrix::I(), 0, 10);
  EXPECT_NE(scoped_flags.flags(), &flags);
  EXPECT_EQ(scoped_flags.flags()->getAlpha(), SkMulDiv255Round(255, 10));
}

TEST(ScopedRasterFlagsTest, ThinAliasedStroke) {
  PaintFlags flags;
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setStrokeWidth(1);
  flags.setAntiAlias(false);

  struct {
    SkMatrix ctm;
    uint8_t alpha;

    bool expect_same_flags;
    bool expect_aa;
    float expect_stroke_width;
    uint8_t expect_alpha;
  } tests[] = {
      // No downscaling                    => no stroke change.
      {SkMatrix::MakeScale(1.0f, 1.0f), 255, true, false, 1.0f, 0xFF},
      // Symmetric downscaling             => modulated hairline stroke.
      {SkMatrix::MakeScale(0.5f, 0.5f), 255, false, false, 0.0f, 0x80},
      // Symmetric downscaling w/ alpha    => modulated hairline stroke.
      {SkMatrix::MakeScale(0.5f, 0.5f), 127, false, false, 0.0f, 0x40},
      // Anisotropic scaling              => AA stroke.
      {SkMatrix::MakeScale(0.5f, 1.5f), 255, false, true, 1.0f, 0xFF},
  };

  for (const auto& test : tests) {
    ScopedRasterFlags scoped_flags(&flags, nullptr, test.ctm, 0, test.alpha);
    ASSERT_TRUE(scoped_flags.flags());

    EXPECT_EQ(scoped_flags.flags() == &flags, test.expect_same_flags);
    EXPECT_EQ(scoped_flags.flags()->isAntiAlias(), test.expect_aa);
    EXPECT_EQ(scoped_flags.flags()->getStrokeWidth(), test.expect_stroke_width);
    EXPECT_EQ(scoped_flags.flags()->getAlpha(), test.expect_alpha);
  }
}

}  // namespace cc
