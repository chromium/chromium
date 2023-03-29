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
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/test/base/in_process_browser_test.h"
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
      "browser-ui-tests-verify-pixels");
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

// Use as `polling_interval` for `WaitUntil` when you know the
// predicate should evaluate to true almost immediately, for example after a few
// `PostTask` bounces.
constexpr static base::TimeDelta kCheckOften = base::Milliseconds(1);

using WaitUntilPredicate = base::RepeatingCallback<bool(void)>;

// Waits until `predicate` evaluates to `true`.
// If `predicate` is already true this will return immediately, otherwise
// it will periodically evaluate `predicate` every `polling_interval` time.
//
// Returns true if `predicate` became true, or false if a timeout happens.
//
[[nodiscard]] bool WaitUntil(WaitUntilPredicate predicate,
                             base::TimeDelta polling_interval) {
  // No need to wait if the predicate is already true.
  if (predicate.Run()) {
    return true;
  }

  base::test::TestFuture<void> ready_signal_;
  base::RepeatingTimer timer;
  timer.Start(FROM_HERE, polling_interval,  //
              base::BindLambdaForTesting([&]() {
                if (predicate.Run()) {
                  ready_signal_.SetValue();
                }
              }));

  return ready_signal_.Wait();
}

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

    EXPECT_TRUE(WaitUntil(
        base::BindLambdaForTesting([&]() {
          return root_window_controller.security_curtain_widget_controller() !=
                 nullptr;
        }),
        kCheckOften));

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
    views::ViewSkiaGoldPixelDiff pixel_diff;
    pixel_diff.Init(
        /*screenshot_prefix=*/
        ::testing::UnitTest::GetInstance()->current_test_suite()->name());
    return pixel_diff.CompareViewScreenshot(screenshot, widget.GetRootView());
  }
};

IN_PROC_BROWSER_TEST_P(CurtainModeChromeOsPixelTest, CheckSecurityCurtain) {
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
