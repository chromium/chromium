// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnail/cc/thumbnail_capture_tracker.h"

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace thumbnail {

TEST(ThumbnailCaptureTrackerTest, RunOnDeletion) {
  bool called = false;
  auto callback = base::BindOnce(
      [](bool* called, ThumbnailCaptureTracker* ignored) { *called = true; },
      base::Unretained(&called));
  {
    ThumbnailCaptureTracker tracker(std::move(callback));
    EXPECT_FALSE(called);
    tracker.SetWroteJpeg();
  }
  EXPECT_TRUE(called);
}

TEST(ThumbnailCaptureTrackerTest, RunOnWroteJpeg) {
  base::test::TaskEnvironment task_environment;
  ThumbnailCaptureTracker tracker(base::DoNothing());
  bool output = false;
  auto callback =
      base::BindOnce([](bool* output, bool input) { *output = input; },
                     base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  EXPECT_FALSE(output);
  tracker.SetWroteJpeg();
  EXPECT_TRUE(output);

  // Automatically called after set.
  output = false;
  callback = base::BindOnce([](bool* output, bool input) { *output = input; },
                            base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  task_environment.RunUntilIdle();
  EXPECT_TRUE(output);
}

TEST(ThumbnailCaptureTrackerTest, RunOnMarkCaptureFailed) {
  base::test::TaskEnvironment task_environment;
  ThumbnailCaptureTracker tracker(base::DoNothing());
  bool output = true;
  auto callback =
      base::BindOnce([](bool* output, bool input) { *output = input; },
                     base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  EXPECT_TRUE(output);
  tracker.MarkCaptureFailed();
  EXPECT_FALSE(output);

  // Automatically called after set.
  output = true;
  callback = base::BindOnce([](bool* output, bool input) { *output = input; },
                            base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  task_environment.RunUntilIdle();
  EXPECT_FALSE(output);
}

TEST(ThumbnailCaptureTrackerTest, RunOnMarkJpegFailed) {
  base::test::TaskEnvironment task_environment;
  ThumbnailCaptureTracker tracker(base::DoNothing());
  bool output = true;
  auto callback =
      base::BindOnce([](bool* output, bool input) { *output = input; },
                     base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  EXPECT_TRUE(output);
  tracker.MarkJpegFailed();
  EXPECT_FALSE(output);

  // Automatically called after set.
  output = true;
  callback = base::BindOnce([](bool* output, bool input) { *output = input; },
                            base::Unretained(&output));
  tracker.AddOnJpegFinishedCallback(std::move(callback));
  task_environment.RunUntilIdle();
  EXPECT_FALSE(output);
}

}  // namespace thumbnail
