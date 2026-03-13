// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/report_unsafe_site/screenshot_taker.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"

namespace feedback {

namespace {

struct CallbackHolder {
  base::OnceClosure callback;
};

void CopyFromSurface(
    CallbackHolder* finish_taking_screenshot_callback_holder,
    const gfx::Rect& src_rect,
    const gfx::Size& output_size,
    base::OnceCallback<void(const content::CopyFromSurfaceResult&)> callback) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorBLUE);
  finish_taking_screenshot_callback_holder->callback = base::BindOnce(
      std::move(callback), viz::CopyOutputBitmapWithMetadata(bitmap));
}

void OnGotScreenshot(SkBitmap* out_screenshot,
                     base::OnceClosure quit_closure,
                     const SkBitmap& screenshot) {
  if (out_screenshot) {
    *out_screenshot = screenshot;
  }
  std::move(quit_closure).Run();
}

}  // namespace

class FeedbackScreenshotTakerTest : public testing::Test {
 public:
  FeedbackScreenshotTakerTest() = default;
  ~FeedbackScreenshotTakerTest() override = default;

  void CheckComputeTargetSize(gfx::Size source_size,
                              gfx::Size expected_target_size) {
    const gfx::Size kMaxSize(200, 100);
    gfx::Size target_size =
        ScreenshotTaker::ComputeTargetSize(source_size, kMaxSize);
    EXPECT_EQ(target_size, expected_target_size);
  }

  std::unique_ptr<ScreenshotTaker> StartScreenshotTaker(
      CallbackHolder& finish_taking_screenshot_callback_holder) {
    auto screenshot_taker =
        base::WrapUnique<ScreenshotTaker>(new ScreenshotTaker(
            base::BindOnce(
                &CopyFromSurface,
                base::Unretained(&finish_taking_screenshot_callback_holder)),
            /*view_size=*/gfx::Size(100, 100)));
    return screenshot_taker;
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(FeedbackScreenshotTakerTest, NullRenderWidgetHostView) {
  auto screenshot_taker = ScreenshotTaker::Start(nullptr);
  SkBitmap out_screenshot;
  base::RunLoop run_loop;
  screenshot_taker->SetCallback(base::BindOnce(
      &OnGotScreenshot, &out_screenshot, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_TRUE(out_screenshot.drawsNothing());
}

TEST_F(FeedbackScreenshotTakerTest, ScreenshotTakenPriorToCallbackSet) {
  CallbackHolder finish_screenshot_callback_holder;
  auto screenshot_taker =
      StartScreenshotTaker(finish_screenshot_callback_holder);
  std::move(finish_screenshot_callback_holder.callback).Run();
  SkBitmap out_screenshot;
  base::RunLoop run_loop;
  screenshot_taker->SetCallback(base::BindOnce(
      &OnGotScreenshot, &out_screenshot, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(out_screenshot.drawsNothing());
}

TEST_F(FeedbackScreenshotTakerTest, ScreenshotTakenAfterCallbackSet) {
  CallbackHolder finish_screenshot_callback_holder;
  auto screenshot_taker =
      StartScreenshotTaker(finish_screenshot_callback_holder);
  SkBitmap out_screenshot;
  base::RunLoop run_loop;
  screenshot_taker->SetCallback(base::BindOnce(
      &OnGotScreenshot, &out_screenshot, run_loop.QuitClosure()));

  std::move(finish_screenshot_callback_holder.callback).Run();
  run_loop.Run();
  EXPECT_FALSE(out_screenshot.drawsNothing());
}

TEST_F(FeedbackScreenshotTakerTest, ComputeTargetSize_SourceIsSmallerThanMax) {
  CheckComputeTargetSize(/*source_size=*/gfx::Size(100, 50),
                         /*expected_target_size=*/gfx::Size(100, 50));
}

TEST_F(FeedbackScreenshotTakerTest, ComputeTargetSize_SourceIsWiderThanMax) {
  // Should maintain aspect ratio.
  CheckComputeTargetSize(/*source_size=*/gfx::Size(400, 50),
                         /*expected_target_size=*/gfx::Size(200, 25));
}

TEST_F(FeedbackScreenshotTakerTest, ComputeTargetSize_SourceIsTallerThanMax) {
  // Should maintain aspect ratio.
  CheckComputeTargetSize(/*source_size=*/gfx::Size(200, 200),
                         /*expected_target_size=*/gfx::Size(100, 100));
}

}  // namespace feedback
