// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <array>
#include <vector>

#include "ash/public/cpp/file_preview/file_preview_controller.h"
#include "ash/public/cpp/file_preview/file_preview_image_skia_source.h"
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
  SkColor last_pixel_color() const { return last_pixel_color_; }

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    // Create a test file with dummy data in it. This data does not actually
    // need to be valid gif data, because the `TestImageDecoder` will hijack
    // decoding and return test frames. It must contain some data, though, to
    // prevent the `image_util::DecodeAnimationData()` from exiting early.
    ASSERT_TRUE(base::WriteFile(TestFilePath(), "foobar"));
  }

  base::FilePath TestFilePath() const {
    return scoped_temp_dir_.GetPath().Append("test_file.gif");
  }

  void SetUpFilePreviewController(base::FilePath path) {
    controller_ = std::make_unique<FilePreviewController>(std::move(path),
                                                          gfx::Size(1, 1));
    invalidation_subscription_ =
        controller_->AddInvalidationCallback(base::BindLambdaForTesting([&]() {
          SkBitmap bitmap = GetBitmap();
          last_pixel_color_ =
              bitmap.isNull() ? SK_ColorTRANSPARENT : bitmap.getColor(0, 0);
        }));
  }

  SkBitmap GetBitmap() {
    const auto* bitmap_ptr = controller()->GetImageSkiaForTest().bitmap();
    return bitmap_ptr ? SkBitmap(*bitmap_ptr) : SkBitmap();
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
  std::unique_ptr<FilePreviewController> controller_;
  base::MockRepeatingCallback<SkBitmap()> mock_image_callback_;
  TestImageDecoder decoder_;
  SkColor last_pixel_color_ = SK_ColorTRANSPARENT;
  base::CallbackListSubscription invalidation_subscription_;
};

TEST_F(FilePreviewTest, IsNullForBadPath) {
  SetUpFilePreviewController(base::FilePath("does_not_exist.gif"));
  auto bitmap = GetBitmap();
  EXPECT_TRUE(bitmap.isNull());

  // Since this file does not exist, loading it should fail and the image
  // should continue to be empty.
  WaitForInvalidation();
  bitmap = GetBitmap();
  EXPECT_TRUE(bitmap.isNull());
}

TEST_F(FilePreviewTest, PlaybackModeChange) {
  SetUpFilePreviewController(base::FilePath(TestFilePath()));

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
  auto pixel_color = GetBitmap().getColor(0, 0);
  EXPECT_EQ(pixel_color, kTestColors[0]);
}

}  // namespace ash
