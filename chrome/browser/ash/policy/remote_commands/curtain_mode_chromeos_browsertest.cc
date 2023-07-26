// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/remote_maintenance_curtain_view.h"
#include "ash/curtain/security_curtain_widget_controller.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/check_deref.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "remoting/host/curtain_mode_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/view_skia_gold_pixel_diff.h"
#include "ui/views/view_observer.h"
#include "ui/views/widget/widget.h"

namespace remoting {

namespace {

enum class ColorScheme {
  kDark,
  kLight,
};

bool ArePixelTestsEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kVerifyPixels);
}

std::string GetScreenshotName(ColorScheme color_scheme) {
  return base::StringPrintf(
      "curtain_%s_rev0", color_scheme == ColorScheme::kDark ? "dark" : "light");
}

// Helper class to wait until WebUI content finishes loading.
class LoadWaiter : public ash::AshWebView::Observer {
 public:
  explicit LoadWaiter(ash::AshWebView& view) : observation_(this) {
    observation_.Observe(&view);
  }
  ~LoadWaiter() override = default;

  // Blocks until the WebUI content has finished loading.
  [[nodiscard]] bool Wait() { return signal_.Wait(); }

 private:
  // `ash::AshWebView::Observer` implementation:
  void DidStopLoading() override { signal_.SetValue(); }

  base::test::TestFuture<void> signal_;
  base::ScopedObservation<ash::AshWebView, ash::AshWebView::Observer>
      observation_;
};

}  // namespace

class CurtainModeChromeOsPixelTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<ColorScheme> {
 public:
  CurtainModeChromeOsPixelTest() = default;
  CurtainModeChromeOsPixelTest(const CurtainModeChromeOsPixelTest&) = delete;
  CurtainModeChromeOsPixelTest& operator=(const CurtainModeChromeOsPixelTest&) =
      delete;
  ~CurtainModeChromeOsPixelTest() override = default;

  void EnableColorScheme(ColorScheme scheme) {
    bool is_dark = (scheme == ColorScheme::kDark);
    ash::DarkLightModeController::Get()->SetDarkModeEnabledForTest(is_dark);
  }

  views::Widget& WaitForCurtainWidget() {
    auto& root_window_controller =
        CHECK_DEREF(ash::Shell::GetPrimaryRootWindowController());

    EXPECT_TRUE(base::test::RunUntil([&]() {
      return root_window_controller.security_curtain_widget_controller() !=
             nullptr;
    }));

    return root_window_controller.security_curtain_widget_controller()
        ->GetWidget();
  }

  void WaitForWebUiToLoad(views::Widget& curtain_widget) {
    ash::AshWebView* view =
        static_cast<ash::AshWebView*>(curtain_widget.GetRootView()->GetViewByID(
            ash::curtain::kRemoteMaintenanceCurtainAshWebViewId));
    ASSERT_NE(view, nullptr);

    ASSERT_TRUE(LoadWaiter{*view}.Wait());
  }

  [[nodiscard]] bool CompareWithScreenshot(const views::Widget& widget,
                                           const std::string& screenshot) {
    views::ViewSkiaGoldPixelDiff pixel_diff(
        /*screenshot_prefix=*/
        ::testing::UnitTest::GetInstance()->current_test_suite()->name());
    return pixel_diff.CompareViewScreenshot(screenshot, widget.GetRootView());
  }
};

// This test is flaky because we often take the screenshot before the image
// is displayed. So far we haven't managed to find a reliable signal that is
// triggered after the image is displayed - even the JS on-ready signals trigger
// before the image is shown :(
// TODO(b/284233962): Find a reliable signal.
IN_PROC_BROWSER_TEST_P(CurtainModeChromeOsPixelTest,
                       DISABLED_CheckSecurityCurtain) {
  if (!ArePixelTestsEnabled()) {
    return;
  }

  const ColorScheme color_scheme = GetParam();
  EnableColorScheme(color_scheme);

  CurtainModeChromeOs curtain(
      base::SingleThreadTaskRunner::GetCurrentDefault());
  curtain.Activate();

  views::Widget& widget = WaitForCurtainWidget();
  WaitForWebUiToLoad(widget);

  EXPECT_TRUE(CompareWithScreenshot(widget, GetScreenshotName(color_scheme)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         CurtainModeChromeOsPixelTest,
                         testing::Values(ColorScheme::kDark,
                                         ColorScheme::kLight));

}  // namespace remoting
