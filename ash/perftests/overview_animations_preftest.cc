// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/shell/content/test/ash_content_test.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "ui/base/test/ui_controls.h"

// Test overview enter/exit animations with following conditions
// int: number of windows : 2, 8
// bool: the window type (browser window with content, or non browser window)
// bool: tablet mode, if true.
// TODO(oshima): Add Tablet/SplitView mode.
class OverviewAnimationsTest
    : public AshContentTest,
      public testing::WithParamInterface<::testing::tuple<int, bool, bool>> {
 public:
  OverviewAnimationsTest() = default;
  ~OverviewAnimationsTest() override = default;

  // AshContentTest:
  void SetUpOnMainThread() override {
    AshContentTest::SetUpOnMainThread();

    int n_browsers = std::get<0>(GetParam());
    bool browser = std::get<1>(GetParam());
    tablet_mode_ = std::get<2>(GetParam());

    if (tablet_mode_)
      ash::ShellTestApi().SetTabletModeEnabledForTest(true);

    const GURL google_url("https://www.google.com");

    for (int i = 0; i < n_browsers; ++i) {
      auto* window = browser ? CreateBrowserWindow(google_url)
                             : CreateBrowserWindow(GURL());
      if (!window_)
        window_ = window;
    }

    float cost_per_browser = browser ? 0.5 : 0.1;
    int wait_seconds = (base::SysInfo::IsRunningOnChromeOS() ? 5 : 0) +
                       n_browsers * cost_per_browser;
    base::RunLoop run_loop;
    base::PostDelayedTask(FROM_HERE, run_loop.QuitClosure(),
                          base::TimeDelta::FromSeconds(wait_seconds));
    run_loop.Run();
  }

  // UIPerformanceTest:
  std::vector<std::string> GetUMAHistogramNames() const override {
    if (tablet_mode_) {
      return {"Ash.Overview.AnimationSmoothness.Enter.TabletMode",
              "Ash.Overview.AnimationSmoothness.Exit.TabletMode"};
    }
    return {"Ash.Overview.AnimationSmoothness.Enter.ClamshellMode",
            "Ash.Overview.AnimationSmoothness.Exit.ClamshellMode"};
  }

  aura::Window* window() { return window_; }

 private:
  aura::Window* window_ = nullptr;
  bool tablet_mode_ = false;
  DISALLOW_COPY_AND_ASSIGN(OverviewAnimationsTest);
};

ASH_CONTENT_TEST_P(OverviewAnimationsTest, EnterExit) {
  TRACE_EVENT_ASYNC_BEGIN0("ui", "Interaction.ui_Overview", this);
  // Browser window is used just to identify display.
  ui_controls::SendKeyPress(window(), ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kEnterAnimationComplete);
  // TODO(oshima): Wait until frame animation ends.
  ui_controls::SendKeyPress(window(), ui::VKEY_MEDIA_LAUNCH_APP1,
                            /*control=*/false,
                            /*shift=*/false,
                            /*alt=*/false,
                            /*command=*/false);
  ash::ShellTestApi().WaitForOverviewAnimationState(
      ash::OverviewAnimationState::kExitAnimationComplete);
  TRACE_EVENT_ASYNC_END0("ui", "Interaction.ui_Overview", this);
}

INSTANTIATE_TEST_SUITE_P(,
                         OverviewAnimationsTest,
                         ::testing::Combine(::testing::Values(2, 8),
                                            /*blank=*/testing::Bool(),
                                            /*tablet=*/testing::Bool()));
