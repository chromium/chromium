// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/paint_image.h"

#include "base/test/gtest_util.h"
#include "cc/paint/paint_image_builder.h"
#include "cc/test/fake_paint_image_generator.h"
#include "cc/test/skia_common.h"
#include "cc/test/test_paint_worklet_input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

TEST(PaintImageTest, Subsetting) {
  PaintImage image = CreateDiscardablePaintImage(gfx::Size(100, 100));
  EXPECT_EQ(image.width(), 100);
  EXPECT_EQ(image.height(), 100);

  PaintImage subset_rect_1 = PaintImageBuilder::WithCopy(image)
                                 .make_subset(gfx::Rect(25, 25, 50, 50))
                                 .TakePaintImage();
  EXPECT_EQ(subset_rect_1.width(), 50);
  EXPECT_EQ(subset_rect_1.height(), 50);
  EXPECT_EQ(subset_rect_1.subset_rect_, gfx::Rect(25, 25, 50, 50));

  PaintImage subset_rect_2 = PaintImageBuilder::WithCopy(subset_rect_1)
                                 .make_subset(gfx::Rect(25, 25, 25, 25))
                                 .TakePaintImage();
  EXPECT_EQ(subset_rect_2.width(), 25);
  EXPECT_EQ(subset_rect_2.height(), 25);
  EXPECT_EQ(subset_rect_2.subset_rect_, gfx::Rect(50, 50, 25, 25));

  EXPECT_EQ(image, PaintImageBuilder::WithCopy(image)
                       .make_subset(gfx::Rect(100, 100))
                       .TakePaintImage());
  EXPECT_DCHECK_DEATH(PaintImageBuilder::WithCopy(subset_rect_2)
                          .make_subset(gfx::Rect(10, 10, 25, 25))
                          .TakePaintImage());
  EXPECT_DCHECK_DEATH(PaintImageBuilder::WithCopy(image)
                          .make_subset(gfx::Rect())
                          .TakePaintImage());
}

TEST(PaintImageTest, DecodesCorrectFrames) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};
  sk_sp<FakePaintImageGenerator> generator =
      sk_make_sp<FakePaintImageGenerator>(SkImageInfo::MakeN32Premul(10, 10),
                                          frames);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(generator)
                         .TakePaintImage();

  // The recorded index is 0u but ask for 1u frame.
  SkImageInfo info = SkImageInfo::MakeN32Premul(10, 10);
  std::vector<size_t> memory(info.computeMinByteSize());
  image.Decode(memory.data(), &info, nullptr, 1u,
               PaintImage::kDefaultGeneratorClientId);
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();

  // Subsetted.
  PaintImage subset_image = PaintImageBuilder::WithCopy(image)
                                .make_subset(gfx::Rect(0, 0, 5, 5))
                                .TakePaintImage();
  SkImageInfo subset_info = info.makeWH(5, 5);
  subset_image.Decode(memory.data(), &subset_info, nullptr, 1u,
                      PaintImage::kDefaultGeneratorClientId);
  ASSERT_EQ(generator->frames_decoded().size(), 1u);
  EXPECT_EQ(generator->frames_decoded().count(1u), 1u);
  generator->reset_frames_decoded();

  // Not N32 color type.
  info.makeColorType(kRGB_565_SkColorType);
  memory = std::vector<size_t>(info.computeMinByteSize());
  image.Decode(memory.data(), &info, nullptr, 1u,
               PaintImage::kDefaultGeneratorClientId);
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

  PaintImage subset = PaintImageBuilder::WithCopy(image)
                          .make_subset(gfx::Rect(8, 8))
                          .TakePaintImage();
  EXPECT_EQ(subset.GetSupportedDecodeSize(supported_sizes[0]),
            SkISize::Make(8, 8));
}

TEST(PaintImageTest, GetSkImageForFrameNotGeneratorBacked) {
  PaintImage image = CreateBitmapImage(gfx::Size(10, 10));
  EXPECT_EQ(image.GetSkImage(),
            image.GetSkImageForFrame(PaintImage::kDefaultFrameIndex,
                                     PaintImage::GetNextGeneratorClientId()));
}

TEST(PaintImageTest, DecodeToYuv420NoAlpha) {
  const SkISize full_size = SkISize::Make(10, 10);
  const SkISize uv_size = SkISize::Make(5, 5);
  SkYUVASizeInfo yuva_size_info;
  yuva_size_info.fSizes[SkYUVAIndex::kY_Index] = full_size;
  yuva_size_info.fWidthBytes[SkYUVAIndex::kY_Index] =
      base::checked_cast<size_t>(full_size.width());

  yuva_size_info.fSizes[SkYUVAIndex::kU_Index] = uv_size;
  yuva_size_info.fWidthBytes[SkYUVAIndex::kU_Index] =
      base::checked_cast<size_t>(uv_size.width());

  yuva_size_info.fSizes[SkYUVAIndex::kV_Index] = uv_size;
  yuva_size_info.fWidthBytes[SkYUVAIndex::kV_Index] =
      base::checked_cast<size_t>(uv_size.width());

  yuva_size_info.fSizes[SkYUVAIndex::kA_Index] = SkISize::MakeEmpty();
  yuva_size_info.fWidthBytes[SkYUVAIndex::kA_Index] = 0u;

  sk_sp<FakePaintImageGenerator> yuv_generator =
      sk_make_sp<FakePaintImageGenerator>(SkImageInfo::MakeN32Premul(full_size),
                                          yuva_size_info);
  PaintImage image = PaintImageBuilder::WithDefault()
                         .set_id(PaintImage::GetNextId())
                         .set_paint_image_generator(yuv_generator)
                         .TakePaintImage();

  std::vector<uint8_t> memory(yuva_size_info.computeTotalBytes());
  void* planes[SkYUVASizeInfo::kMaxCount];
  yuva_size_info.computePlanes(memory.data(), planes);

  SkYUVASizeInfo image_yuv_size_info;
  SkYUVAIndex image_plane_indices[SkYUVAIndex::kIndexCount];
  ASSERT_TRUE(image.IsYuv(&image_yuv_size_info, image_plane_indices));
  ASSERT_EQ(yuva_size_info, image_yuv_size_info);

  SkYUVAIndex plane_indices[SkYUVAIndex::kIndexCount];
  image.DecodeYuv(planes, 1u /* frame_index */,
                  PaintImage::kDefaultGeneratorClientId, yuva_size_info,
                  plane_indices);
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
                               .set_paint_worklet_input(std::move(input))
                               .TakePaintImage();
  EXPECT_TRUE(paint_image.paint_worklet_input());
  EXPECT_EQ(paint_image.width(), size.width());
  EXPECT_EQ(paint_image.height(), size.height());
}

}  // namespace cc
