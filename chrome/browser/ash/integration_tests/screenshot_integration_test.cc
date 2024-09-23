// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/shell.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/chromeos/crosier/annotations.h"
#include "chrome/test/base/chromeos/crosier/chromeos_integration_test_mixin.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "gpu/config/gpu_finch_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/gfx/color_analysis.h"

namespace {

class ScreenshotIntegrationTest : public MixinBasedInProcessBrowserTest,
                                  public testing::WithParamInterface<bool> {
 public:
  ScreenshotIntegrationTest() {
    if (UseVulkan()) {
      // Check for board support because enabling the ScopedFeatureList,
      // otherwise GPU process initialization will crash before the test body.
      if (crosier::HasRequirement(crosier::Requirement::kVulkan)) {
        feature_list_.InitAndEnableFeature(features::kVulkan);
      } else {
        skip_test_ = true;
      }
    } else {
      feature_list_.InitAndDisableFeature(features::kVulkan);
    }
  }

  bool UseVulkan() { return GetParam(); }

  // MixinBasedInProcessBrowserTest:
  void TearDownOnMainThread() override {
    // Clean up even if the test was skipped.
    browser()->window()->Close();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  bool skip_test_ = false;
  ChromeOSIntegrationTestMixin chromeos_integration_test_mixin_{&mixin_host_};
};

INSTANTIATE_TEST_SUITE_P(Vulkan, ScreenshotIntegrationTest, testing::Bool());

// TODO(b/349252684): Sometimes fails with "CRTC not found. Is the screen on?"
// despite the code below that turns on the screen.
IN_PROC_BROWSER_TEST_P(ScreenshotIntegrationTest, DISABLED_AverageColor) {
  if (skip_test_) {
    GTEST_SKIP() << "Skipping test because target doesn't support Vulkan.";
  }

  // Ensure the display is powered on, otherwise the screenshot will fail.
  base::RunLoop run_loop;
  ash::Shell::Get()->display_configurator()->SetDisplayPower(
      chromeos::DISPLAY_POWER_ALL_ON,
      display::DisplayConfigurator::kSetDisplayPowerForceProbe,
      base::BindLambdaForTesting([&](bool success) {
        ASSERT_TRUE(success);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Maximize the browser window.
  ASSERT_TRUE(browser());
  browser()->window()->Maximize();

  // Load a page with a solid red background.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<body bgcolor=red></body>")));

  // We don't know when the frame's pixels will be scanned out, so take
  // screenshots in a loop until we get a valid one.
  SkColor dominant_color;
  bool success = false;
  for (int i = 0; i < 10; ++i) {
    // Sleep for 1 second.
    base::RunLoop run_loop2;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop2.QuitClosure(), base::Seconds(1));
    run_loop2.Run();

    // Use the command-line screenshot utility to capture the screen.
    auto result =
        TestSudoHelperClient().RunCommand("screenshot /tmp/screen.png");
    ASSERT_EQ(result.return_code, 0) << result.output;

    // Load the PNG screenshot.
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::optional<std::vector<uint8_t>> image_png =
        base::ReadFileToBytes(base::FilePath("/tmp/screen.png"));
    ASSERT_TRUE(image_png.has_value());

    // Compute the dominant color.
    dominant_color = color_utils::CalculateKMeanColorOfPNG(*image_png);

    // If the color matches the red page background, we're done.
    if (dominant_color == SK_ColorRED) {
      success = true;
      break;
    }

    // The screen may not yet have valid pixels yet.
    LOG(WARNING) << "Dominant color " << std::hex << dominant_color
                 << " does not match expected " << SK_ColorRED;
  }
  EXPECT_TRUE(success) << "Final screenshot had invalid dominant color "
                       << std::hex << dominant_color;
}

}  // namespace
