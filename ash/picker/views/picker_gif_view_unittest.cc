// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_gif_view.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/image_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace {

constexpr gfx::Size kImageSize(100, 100);

image_util::AnimationFrame CreateGifFrame(base::TimeDelta duration) {
  return {.image = image_util::CreateEmptyImage(kImageSize),
          .duration = duration};
}

void FetchGifFrames(std::vector<image_util::AnimationFrame> frames,
                    PickerGifView::FramesFetchedCallback callback) {
  std::move(callback).Run(frames);
}

gfx::ImageSkia GetImage(const PickerGifView& gif_view) {
  return gif_view.GetImageModel().GetImage().AsImageSkia();
}

TEST(PickerGifViewTest, ImageSize) {
  base::test::SingleThreadTaskEnvironment task_environment;

  constexpr gfx::Size kPreferredImageSize(200, 300);
  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(30)),
      CreateGifFrame(base::Milliseconds(40))};
  PickerGifView gif_view(base::BindOnce(&FetchGifFrames, frames),
                         kPreferredImageSize);

  EXPECT_EQ(gif_view.GetImageModel().Size(), kPreferredImageSize);
  EXPECT_EQ(gif_view.GetPreferredSize(), kPreferredImageSize);
}

TEST(PickerGifViewTest, FrameDurations) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(30)),
      CreateGifFrame(base::Milliseconds(40)),
      CreateGifFrame(base::Milliseconds(50))};
  PickerGifView gif_view(base::BindOnce(&FetchGifFrames, frames), kImageSize);
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  task_environment.FastForwardBy(frames[0].duration);
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[1].image));

  task_environment.FastForwardBy(frames[1].duration);
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[2].image));

  task_environment.FastForwardBy(frames[2].duration);
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));
}

TEST(PickerGifViewTest, AdjustsShortFrameDurations) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(0)),
      CreateGifFrame(base::Milliseconds(30))};
  PickerGifView gif_view(base::BindOnce(&FetchGifFrames, frames), kImageSize);

  // We use a duration of 100ms for frames that specify a duration of <= 10ms
  // (to follow the behavior of blink).
  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  task_environment.FastForwardBy(base::Milliseconds(60));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[1].image));
}

}  // namespace
}  // namespace ash
