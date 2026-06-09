// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_window_occlusion_helper_base.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

using OcclusionState =
    AutoPictureInPictureWindowOcclusionHelperBase::OcclusionState;

class OcclusionStateWaiter {
 public:
  OcclusionStateWaiter() = default;
  ~OcclusionStateWaiter() = default;

  // Waits until the occlusion state matches the expected state. Returns true if
  // the occlusion state matches before timeout.
  bool WaitForOcclusionState(OcclusionState expected_state) {
    // Should not attempt to wait while already waiting.
    CHECK(!wait_loop_);

    if (occlusion_state_.has_value() && *occlusion_state_ == expected_state) {
      return true;
    }

    expected_state_ = expected_state;
    wait_loop_ = std::make_unique<base::RunLoop>();
    wait_loop_->Run();
    wait_loop_.reset();

    return (occlusion_state_.has_value() &&
            occlusion_state_ == expected_state_);
  }

  void OnOcclusionStateChanged(OcclusionState occlusion_state) {
    occlusion_state_ = occlusion_state;
    if (wait_loop_) {
      CHECK(expected_state_.has_value());
      if (*expected_state_ == occlusion_state) {
        wait_loop_->Quit();
      }
    }
  }

  void set_occlusion_state(OcclusionState state) { occlusion_state_ = state; }

 private:
  std::optional<OcclusionState> occlusion_state_;
  std::optional<OcclusionState> expected_state_;
  std::unique_ptr<base::RunLoop> wait_loop_;
};

class AutoPictureInPictureWindowOcclusionInteractiveUiTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
#if BUILDFLAG(IS_MAC)
    int version = base::mac::MacOSVersion();
    if ((version >= 130000 && version < 130300) || version >= 260000) {
      GTEST_SKIP() << "Manual window occlusion detection is not supported on this macOS version.";
    }
#endif
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());

    // Make WebContents respect occlusion state. We call this here instead of in
    // an overridden `SetUpCommandLine()` because `SetUpCommandLine()` is run
    // before BrowserTestBase adds the switch we want to remove.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);

    InProcessBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureWindowOcclusionInteractiveUiTest,
                       DetectsOcclusion) {
  // Open the first window.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.com", "/title1.html")));
  content::WebContents* web_contents1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open the second window.
  Browser* browser2 = CreateBrowser(browser()->profile());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser2, embedded_test_server()->GetURL("b.com", "/title2.html")));

  // Position the windows so they don't overlap.
  gfx::Rect browser_position_1(0, 0, 500, 500);
  gfx::Rect browser_position_2(600, 0, 500, 500);
  browser()->GetWindow()->SetBounds(browser_position_1);
  browser2->GetWindow()->SetBounds(browser_position_2);

  OcclusionStateWaiter occlusion_state_waiter;
  auto occlusion_helper = AutoPictureInPictureWindowOcclusionHelperBase::Create(
      web_contents1,
      base::BindRepeating(&OcclusionStateWaiter::OnOcclusionStateChanged,
                          base::Unretained(&occlusion_state_waiter)));
  occlusion_helper->StartObserving();
  occlusion_state_waiter.set_occlusion_state(
      occlusion_helper->GetOcclusionState());

  // Initially, the window should be visible.
  EXPECT_EQ(occlusion_helper->GetOcclusionState(), OcclusionState::kVisible);

  // Move the second window to completely occlude the first window.
  browser2->GetWindow()->SetBounds(browser_position_1);

  // The helper should report that the window is occluded.
  EXPECT_TRUE(
      occlusion_state_waiter.WaitForOcclusionState(OcclusionState::kOccluded));
  EXPECT_EQ(occlusion_helper->GetOcclusionState(), OcclusionState::kOccluded);

  // Move the second window back to not overlapping.
  browser2->GetWindow()->SetBounds(browser_position_2);

  // The helper should report that the window is visible.
  EXPECT_TRUE(
      occlusion_state_waiter.WaitForOcclusionState(OcclusionState::kVisible));
  EXPECT_EQ(occlusion_helper->GetOcclusionState(), OcclusionState::kVisible);

  // Minimize the first window.
  browser()->GetWindow()->Minimize();

  // The helper should report that the window is hidden.
  EXPECT_TRUE(
      occlusion_state_waiter.WaitForOcclusionState(OcclusionState::kHidden));
  EXPECT_EQ(occlusion_helper->GetOcclusionState(), OcclusionState::kHidden);
}

}  // namespace
