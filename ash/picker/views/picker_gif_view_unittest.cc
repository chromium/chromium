// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_gif_view.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/image_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace ash {
namespace {

constexpr gfx::Size kImageSize(100, 100);

image_util::AnimationFrame CreateGifFrame(base::TimeDelta duration) {
  return {.image = image_util::CreateEmptyImage(kImageSize),
          .duration = duration};
}

gfx::ImageSkia GetImage(const PickerGifView& gif_view) {
  return gif_view.GetImageModel().GetImage().AsImageSkia();
}

class GifAssetFetcher {
 public:
  GifAssetFetcher() = default;
  GifAssetFetcher(const GifAssetFetcher&) = delete;
  GifAssetFetcher& operator=(const GifAssetFetcher&) = delete;
  ~GifAssetFetcher() = default;

  PickerGifView::FramesFetcher GetFramesFetcher() {
    return base::BindLambdaForTesting(
        [this](PickerGifView::FramesFetchedCallback callback) {
          frames_fetched_callback_ = std::move(callback);
        });
  }

  void CompleteFetchFrames(
      std::vector<image_util::AnimationFrame> frames = {}) {
    std::move(frames_fetched_callback_).Run(frames);
  }

  PickerGifView::PreviewImageFetcher GetPreviewImageFetcher() {
    return base::BindLambdaForTesting(
        [this](PickerGifView::PreviewImageFetchedCallback callback) {
          preview_image_fetched_callback_ = std::move(callback);
        });
  }

  void CompleteFetchPreviewImage(
      const gfx::ImageSkia& preview_image = gfx::ImageSkia()) {
    std::move(preview_image_fetched_callback_).Run(preview_image);
  }

 private:
  PickerGifView::FramesFetchedCallback frames_fetched_callback_;
  PickerGifView::PreviewImageFetchedCallback preview_image_fetched_callback_;
};

TEST(PickerGifViewTest, PreferredHeightPreservesAspectRatio) {
  constexpr gfx::Size kOriginalGifDimensions(100, 200);
  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(),
                         kOriginalGifDimensions);

  EXPECT_EQ(gif_view.GetHeightForWidth(50), 100);
}

TEST(PickerGifViewTest, CorrectSizeBeforePreviewFetched) {
  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  EXPECT_EQ(gif_view.GetImageModel().Size(), kImageSize);
}

TEST(PickerGifViewTest, ShowsPreviewImageWhenFramesNotFetched) {
  base::test::SingleThreadTaskEnvironment task_environment;

  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  const gfx::ImageSkia preview_image = image_util::CreateEmptyImage(kImageSize);
  asset_fetcher.CompleteFetchPreviewImage(preview_image);

  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(preview_image));
  EXPECT_EQ(gif_view.GetImageModel().Size(), kImageSize);
}

TEST(PickerGifViewTest, ShowsGifFrameAfterFramesAreFetched) {
  base::test::SingleThreadTaskEnvironment task_environment;

  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  asset_fetcher.CompleteFetchPreviewImage(
      image_util::CreateEmptyImage(kImageSize));
  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(30)),
      CreateGifFrame(base::Milliseconds(40))};
  asset_fetcher.CompleteFetchFrames(frames);

  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));
  EXPECT_EQ(gif_view.GetImageModel().Size(), kImageSize);
}

TEST(PickerGifViewTest, ShowsGifFrameIfPreviewAndFramesBothFetched) {
  base::test::SingleThreadTaskEnvironment task_environment;

  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(30)),
      CreateGifFrame(base::Milliseconds(40))};
  asset_fetcher.CompleteFetchFrames(frames);

  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  asset_fetcher.CompleteFetchPreviewImage(
      image_util::CreateEmptyImage(kImageSize));

  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));
}

TEST(PickerGifViewTest, FrameDurations) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(30)),
      CreateGifFrame(base::Milliseconds(40)),
      CreateGifFrame(base::Milliseconds(50))};
  asset_fetcher.CompleteFetchFrames(frames);

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

  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  const std::vector<image_util::AnimationFrame> frames = {
      CreateGifFrame(base::Milliseconds(0)),
      CreateGifFrame(base::Milliseconds(30))};
  asset_fetcher.CompleteFetchFrames(frames);

  // We use a duration of 100ms for frames that specify a duration of <= 10ms
  // (to follow the behavior of blink).
  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  task_environment.FastForwardBy(base::Milliseconds(20));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[0].image));

  task_environment.FastForwardBy(base::Milliseconds(60));
  EXPECT_TRUE(GetImage(gif_view).BackedBySameObjectAs(frames[1].image));
}

TEST(PickerGifViewTest, RecordsTimeToFirstFrameWhenGifIsFetchedFirst) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  task_environment.FastForwardBy(base::Milliseconds(100));
  asset_fetcher.CompleteFetchFrames({CreateGifFrame(base::Milliseconds(0))});
  task_environment.FastForwardBy(base::Milliseconds(200));
  asset_fetcher.CompleteFetchPreviewImage(
      image_util::CreateEmptyImage(kImageSize));

  histogram_tester.ExpectTotalCount("Ash.Picker.TimeToFirstGifFrame", 1);
  histogram_tester.ExpectUniqueTimeSample("Ash.Picker.TimeToFirstGifFrame",
                                          base::Milliseconds(100), 1);
}

TEST(PickerGifViewTest, RecordsTimeToFirstFrameWhenPreviewIsFetchedFirst) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  task_environment.FastForwardBy(base::Milliseconds(100));
  asset_fetcher.CompleteFetchPreviewImage(
      image_util::CreateEmptyImage(kImageSize));
  task_environment.FastForwardBy(base::Milliseconds(200));
  asset_fetcher.CompleteFetchFrames({CreateGifFrame(base::Milliseconds(0))});

  histogram_tester.ExpectTotalCount("Ash.Picker.TimeToFirstGifFrame", 1);
  histogram_tester.ExpectUniqueTimeSample("Ash.Picker.TimeToFirstGifFrame",
                                          base::Milliseconds(100), 1);
}

TEST(PickerGifViewTest, DoesNotRecordTimeToFirstFrameForInvalidGifsOrPreviews) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  base::HistogramTester histogram_tester;
  GifAssetFetcher asset_fetcher;
  PickerGifView gif_view(asset_fetcher.GetFramesFetcher(),
                         asset_fetcher.GetPreviewImageFetcher(), kImageSize);

  task_environment.FastForwardBy(base::Milliseconds(100));
  asset_fetcher.CompleteFetchPreviewImage(gfx::ImageSkia());
  task_environment.FastForwardBy(base::Milliseconds(200));
  asset_fetcher.CompleteFetchFrames({});

  histogram_tester.ExpectTotalCount("Ash.Picker.TimeToFirstGifFrame", 0);
}

}  // namespace
}  // namespace ash
