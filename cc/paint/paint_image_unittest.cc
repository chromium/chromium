// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include <utility>

#include "base/test/gtest_util.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColorSpace.h"

namespace cc {

TEST(PaintImageTest, DecodesCorrectFrames) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::Milliseconds(2)),
      FrameMetadata(true, base::Milliseconds(3))};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(SkImageInfo::MakeN32Premul(10, 10),
                                          frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();

  // When there's no decoded SkImage the color usage defaults to SRGB.
  EXPECT_EQ(image.GetContentColorUsage(), gfx::ContentColorUsage::kSRGB);

  // The recorded index is 0u but ask for 1u frame.
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  std::vector<size_t> memory(info.computeMinByteSize());
  SkPixmap pixmap(info, memory.data(), info.minRowBytes());
  image.Decode(pixmap, 1u, AuxImage::kDefault,
               PaintImage::GetNextGeneratorClientId());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();

  // Not N32 color type.
  info.makeColorType(kRGB_565_SkColorType);
  memory = std::vector<size_t>(info.computeMinByteSize());
  pixmap = SkPixmap(info, memory.data(), info.minRowBytes());
  image.Decode(pixmap, 1u, AuxImage::kDefault,
               PaintImage::GetNextGeneratorClientId());
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();
}

TEST(PaintImageTest, SupportedDecodeSize) {
  SkISize full_size = SkISize::Make(10, 10);
  std::vector<SkISize> supported_sizes = {SkISize::Make(5, 5)};
  std::vector<FrameMetadata> frames = {FrameMetadata()};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(
          SkImageInfo::MakeN32Premul(full_size.width(), full_size.height()),
          frames, true, supported_sizes);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();
  EXPECT_EQ(image.GetSupportedDecodeSize(supported_sizes[0]),
            supported_sizes[0]);
}

TEST(PaintImageTest, GetSkImageForFrameNotGeneratorBacked) {
  PaintImage image = CreateBitmapImage(gfx::Size(10, 10));
  EXPECT_EQ(image.GetSwSkImage(),
            image.GetSkImageForFrame(PaintImage::kDefaultFrameIndex,
                                     PaintImage::GetNextGeneratorClientId()));
}

TEST(PaintImageTest, DecodeToYuv420NoAlpha) {
  const SkISize full_size = SkISize::Make(10, 10);
  SkYUVAInfo yuva_info(full_size, SkYUVAInfo::PlaneConfig::kY_U_V,
                       SkYUVAInfo::Subsampling::k420,
                       kJPEG_Full_SkYUVColorSpace);
  SkYUVAPixmapInfo yuva_pixmap_info(yuva_info,
                                    SkYUVAPixmapInfo::DataType::kUnorm8,
                                    /*row bytes*/ nullptr);
  sk_sp<FakePaintImageGenerator> yuv_generator =
      sk_make_sp<FakePaintImageGenerator>(SkImageInfo::MakeN32Premul(full_size),
                                          yuva_pixmap_info);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(yuv_generator)
                         .TakePaintImage();

  std::vector<uint8_t> memory(yuva_pixmap_info.computeTotalBytes());
  auto pixmaps =
      SkYUVAPixmaps::FromExternalMemory(yuva_pixmap_info, memory.data());

  SkYUVAPixmapInfo image_yuva_pixmap_info;
  ASSERT_TRUE(image.IsYuv(SkYUVAPixmapInfo::SupportedDataTypes::All(),
                          AuxImage::kDefault, &image_yuva_pixmap_info));
  ASSERT_EQ(yuva_pixmap_info, image_yuva_pixmap_info);

  image.DecodeYuv(pixmaps, 1u /* frame_index */, AuxImage::kDefault,
                  PaintImage::GetNextGeneratorClientId());
  ASSERT_EQ(yuv_generator->frames_decoded().size(), 1u);
  EXPECT_EQ(yuv_generator->frames_decoded().count(1u), 1u);
  yuv_generator->reset_frames_decoded();
}

TEST(PaintImageTest, BuildPaintWorkletImage) {
  gfx::SizeF size(100, 50);
  scoped_refptr<TestPaintWorkletInput> input =
      base::MakeRefCounted<TestPaintWorkletInput>(size);
  PaintImage paint_image = PaintImageBuilder::WithDefault()
                               .set_id(1)
                               .set_deferred_paint_record(std::move(input))
                               .TakePaintImage();
  EXPECT_TRUE(paint_image.deferred_paint_record());
  EXPECT_EQ(paint_image.width(), size.width());
  EXPECT_EQ(paint_image.height(), size.height());
  EXPECT_EQ(paint_image.GetContentColorUsage(), gfx::ContentColorUsage::kSRGB);
}

TEST(PaintImageTest, SrgbImage) {
  auto generator = sk_make_sp<FakePaintImageGenerator>(
      SkImageInfo::Make(10, 10, kRGBA_F16_SkColorType, kUnknown_SkAlphaType,
                        gfx::ColorSpace::CreateSRGB().ToSkColorSpace()));
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_is_high_bit_depth(true)
                         .TakePaintImage();
  EXPECT_TRUE(image.is_high_bit_depth());
  EXPECT_EQ(image.GetContentColorUsage(), gfx::ContentColorUsage::kSRGB);
}

TEST(PaintImageTest, HbdImage) {
  auto generator = sk_make_sp<FakePaintImageGenerator>(SkImageInfo::Make(
      10, 10, kRGBA_F16_SkColorType, kUnknown_SkAlphaType,
      gfx::ColorSpace::CreateDisplayP3D65().ToSkColorSpace()));
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_is_high_bit_depth(true)
                         .TakePaintImage();
  EXPECT_TRUE(image.is_high_bit_depth());
  EXPECT_EQ(image.GetContentColorUsage(),
            gfx::ContentColorUsage::kWideColorGamut);
}

TEST(PaintImageTest, PqHdrImage) {
  auto generator = sk_make_sp<FakePaintImageGenerator>(
      SkImageInfo::Make(10, 10, kRGBA_F16_SkColorType, kUnknown_SkAlphaType,
                        gfx::ColorSpace::CreateHDR10().ToSkColorSpace()));
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_is_high_bit_depth(true)
                         .TakePaintImage();
  EXPECT_TRUE(image.is_high_bit_depth());
  EXPECT_EQ(image.GetContentColorUsage(), gfx::ContentColorUsage::kHDR);
}

TEST(PaintImageTest, HlgHdrImage) {
  auto generator = sk_make_sp<FakePaintImageGenerator>(
      SkImageInfo::Make(10, 10, kRGBA_F16_SkColorType, kUnknown_SkAlphaType,
                        gfx::ColorSpace::CreateHLG().ToSkColorSpace()));
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .set_is_high_bit_depth(true)
                         .TakePaintImage();

  EXPECT_TRUE(image.is_high_bit_depth());
  EXPECT_EQ(image.GetContentColorUsage(), gfx::ContentColorUsage::kHDR);
}

}  // namespace cc
