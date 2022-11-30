// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/signout_screenshot_handler.h"

#include <stdint.h>

#include <memory>

#include "ash/glanceables/glanceables_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
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

  // Screenshot is half the size of the desk container in each dimension.
  gfx::Size screenshot_size = handler.screenshot_size_for_test();
  aura::Window* active_desk =
      desks_util::GetActiveDeskContainerForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(screenshot_size.width(), active_desk->bounds().width() / 2);
  EXPECT_EQ(screenshot_size.height(), active_desk->bounds().height() / 2);

  // Screenshot was taken and is not empty.
  EXPECT_TRUE(base::PathExists(screenshot_path_));
  int64_t file_size = 0;
  ASSERT_TRUE(base::GetFileSize(screenshot_path_, &file_size));
  EXPECT_GT(file_size, 0);

  // Screenshot duration was recorded.
  base::TimeDelta duration =
      glanceables_util::GetSignoutScreenshotDurationForTest(
          Shell::Get()->local_state());
  EXPECT_FALSE(duration.is_zero());
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
