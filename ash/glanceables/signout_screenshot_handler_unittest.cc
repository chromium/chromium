// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/signout_screenshot_handler.h"

#include <stdint.h>

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

class SignoutScreenshotHandlerTest : public AshTestBase {
 public:
  SignoutScreenshotHandlerTest() {
    CHECK(temp_dir_.CreateUniqueTempDir());
    screenshot_path_ = temp_dir_.GetPath().AppendASCII("screenshot.png");
  }

  base::ScopedAllowBlockingForTesting allow_blocking_;
  base::ScopedTempDir temp_dir_;
  base::FilePath screenshot_path_;
};

// Tests that a screenshot is taken when there are windows open.
TEST_F(SignoutScreenshotHandlerTest, TakeScreenshotWithWindowOpen) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  SignoutScreenshotHandler handler;
  handler.set_screenshot_path_for_test(screenshot_path_);
  base::RunLoop run_loop;
  handler.TakeScreenshot(run_loop.QuitClosure());
  run_loop.Run();

  // Screenshot was taken and is not empty.
  EXPECT_TRUE(base::PathExists(screenshot_path_));
  int64_t file_size = 0;
  ASSERT_TRUE(base::GetFileSize(screenshot_path_, &file_size));
  EXPECT_GT(file_size, 0);
}

// Tests that no screenshot is taken when no windows are open and the existing
// screenshot is deleted.
TEST_F(SignoutScreenshotHandlerTest, TakeScreenshotWithNoWindows) {
  // Create an empty file to simulate an old screenshot.
  ASSERT_TRUE(base::WriteFile(screenshot_path_, ""));

  SignoutScreenshotHandler handler;
  handler.set_screenshot_path_for_test(screenshot_path_);
  base::RunLoop run_loop;
  handler.TakeScreenshot(run_loop.QuitClosure());
  run_loop.Run();

  // Existing screenshot was deleted.
  EXPECT_FALSE(base::PathExists(screenshot_path_));
}

}  // namespace
}  // namespace ash
