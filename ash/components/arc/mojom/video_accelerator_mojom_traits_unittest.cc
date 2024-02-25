// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/mojom/video_accelerator_mojom_traits.h"

#include <vector>

#include "ash/components/arc/mojom/video_common.mojom.h"
#include "media/base/video_frame_layout.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {

namespace {
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr media::VideoPixelFormat kFormat = media::PIXEL_FORMAT_I420;
constexpr gfx::Size kCodedSize(kWidth, kHeight);
}  // namespace

TEST(VideoAcceleratorStructTraitsTest, ConvertVideoFrameLayout) {
  std::vector<media::ColorPlaneLayout> planes;
  planes.emplace_back(kWidth, 0, kWidth * kHeight);
  planes.emplace_back(kWidth / 2, kWidth * kHeight, kWidth * kHeight / 4);
  planes.emplace_back(kWidth / 2, kWidth * kHeight + kWidth * kHeight / 4,
                      kWidth * kHeight / 4);
  // Choose a non-default value.
  constexpr size_t buffer_addr_align = 128;
  constexpr uint64_t modifier = 0x1234;

  std::optional<media::VideoFrameLayout> layout =
      media::VideoFrameLayout::CreateWithPlanes(kFormat, kCodedSize, planes,
                                                buffer_addr_align, modifier);
  EXPECT_TRUE(layout);

  std::unique_ptr<media::VideoFrameLayout> input =
      std::make_unique<media::VideoFrameLayout>(*layout);
  std::unique_ptr<media::VideoFrameLayout> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrameLayout>(input,
                                                                    output);

  EXPECT_EQ(output->format(), kFormat);
  EXPECT_EQ(output->coded_size(), kCodedSize);
  EXPECT_EQ(output->planes(), planes);
  EXPECT_EQ(output->buffer_addr_align(), buffer_addr_align);
  EXPECT_EQ(output->modifier(), modifier);
}

TEST(VideoAcceleratorStructTraitsTest, ConvertNullVideoFrameLayout) {
  std::unique_ptr<media::VideoFrameLayout> input;
  std::unique_ptr<media::VideoFrameLayout> output;
  mojo::test::SerializeAndDeserialize<arc::mojom::VideoFrameLayout>(input,
                                                                    output);

  EXPECT_FALSE(output);
}

}  // namespace mojo
