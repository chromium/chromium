// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <vector>

#include "ash/public/cpp/file_preview/file_preview_controller.h"
#include "ash/public/cpp/file_preview/file_preview_factory.h"
#include "ash/public/cpp/test/test_image_decoder.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_skia_source.h"

namespace ash {

namespace {

constexpr std::array<SkColor, 4> kTestColors{SK_ColorRED, SK_ColorYELLOW,
                                             SK_ColorBLUE, SK_ColorMAGENTA};
constexpr base::TimeDelta kTestFrameDelay = base::Milliseconds(100);

SkBitmap Create1xBitmap(SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(1, 1);
  bitmap.eraseColor(color);
  return bitmap;
}

std::vector<data_decoder::mojom::AnimationFramePtr> TestFrames() {
  std::vector<data_decoder::mojom::AnimationFramePtr> result;
  for (auto color : kTestColors) {
    result.push_back(data_decoder::mojom::AnimationFrame::New(
        Create1xBitmap(color), kTestFrameDelay));
  }
  return result;
}

}  // namespace

class FilePreviewTest : public testing::Test {
 public:
  FilePreviewTest()
      : decoder_(base::BindRepeating(&TestFrames), mock_image_callback_.Get()) {
  }

  FilePreviewController* controller() { return controller_.get(); }
  ui::ColorProvider* color_provider() { return &color_provider_; }
  ui::ImageModel* model() { return &model_; }
  SkColor last_pixel_color() const { return last_pixel_color_; }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    // Create a test file with dummy data in it. This data does not actually
    // need to be valid gif data, because the `TestImageDecoder` will hijack
    // decoding and return test frames. It must contain some data, though, to
    // prevent the `image_util::DecodeAnimationData()` from exiting early.
    ASSERT_TRUE(base::WriteFile(TestFilePath(), "foobar"));
    color_provider_.GenerateColorMap();
  }

  void SetUpFilePreviewImageModel(base::FilePath path) {
    model_ = FilePreviewFactory::Get()->CreateImageModel(std::move(path),
                                                         gfx::Size(1, 1));

    controller_ = FilePreviewFactory::Get()->GetController(model_);

    invalidation_subscription_ =
        controller_->AddInvalidationCallback(base::BindLambdaForTesting([&]() {
          auto* bitmap = model_.Rasterize(color_provider()).bitmap();
          last_pixel_color_ =
              bitmap->isNull() ? SK_ColorTRANSPARENT : bitmap->getColor(0, 0);
        }));
  }

  base::FilePath TestFilePath() const {
    return scoped_temp_dir_.GetPath().Append("test_file.gif");
  }

  void WaitForInvalidation() {
    base::RunLoop run_loop;
    auto subscription =
        controller()->AddInvalidationCallback(run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::ScopedTempDir scoped_temp_dir_;
  ui::ColorProvider color_provider_;
  base::MockRepeatingCallback<SkBitmap()> mock_image_callback_;
  TestImageDecoder decoder_;
  SkColor last_pixel_color_ = SK_ColorTRANSPARENT;
  base::CallbackListSubscription invalidation_subscription_;
  raw_ptr<FilePreviewController> controller_ = nullptr;
  ui::ImageModel model_;
};

TEST_F(FilePreviewTest, IsNullForBadPath) {
  SetUpFilePreviewImageModel(base::FilePath("does_not_exist.gif"));
  auto image = model()->Rasterize(color_provider());
  EXPECT_TRUE(image.GetRepresentation(1.0f).is_null());

  // Since this file does not exist, loading it should fail and the image
  // should continue to be empty.
  WaitForInvalidation();
  EXPECT_TRUE(image.GetRepresentation(1.0f).is_null());
}

TEST_F(FilePreviewTest, PlaybackModeChange) {
  SetUpFilePreviewImageModel(base::FilePath(TestFilePath()));

  controller()->SetPlaybackMode(
      FilePreviewImageSkiaSource::PlaybackMode::kLoop);

  // Expect frames to automatically update.
  for (auto expected_color : kTestColors) {
    WaitForInvalidation();
    EXPECT_EQ(last_pixel_color(), expected_color);
  }

  // Animation should now loop back to the beginning and continue.
  WaitForInvalidation();
  EXPECT_EQ(last_pixel_color(), kTestColors[0]);
  WaitForInvalidation();
  EXPECT_EQ(last_pixel_color(), kTestColors[1]);

  controller()->SetPlaybackMode(
      FilePreviewImageSkiaSource::PlaybackMode::kFirstFrame);

  EXPECT_EQ(last_pixel_color(), kTestColors[0]);

  // Wait twice the frame delay, and be sure the frame has not changed and no
  // invalidation events have happened in this time.
  auto subscription =
      controller()->AddInvalidationCallback(base::BindLambdaForTesting([&]() {
        ADD_FAILURE() << "Invalidation event detected when playback mode was "
                         "set to kFirstFrame";
      }));

  base::RunLoop run_loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, kTestFrameDelay * 2, run_loop.QuitClosure());
  run_loop.Run();

  auto pixel_color =
      model()->Rasterize(color_provider()).bitmap()->getColor(0, 0);
  EXPECT_EQ(pixel_color, kTestColors[0]);
}

}  // namespace ash
