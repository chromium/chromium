// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screenshot_testing/screenshot_testing_mixin.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace chromeos {

ScreenshotTestingMixin::ScreenshotTestingMixin()
    : enable_test_screenshots_(false) {}

ScreenshotTestingMixin::~ScreenshotTestingMixin() {}

void ScreenshotTestingMixin::SetUpInProcessBrowserTestFixture() {
  enable_test_screenshots_ = screenshot_tester_.TryInitialize();
}

void ScreenshotTestingMixin::SetUpCommandLine(base::CommandLine* command_line) {
  if (enable_test_screenshots_) {
    command_line->AppendSwitch(switches::kEnablePixelOutputInTests);
  }
}

void ScreenshotTestingMixin::RunScreenshotTesting(
    const std::string& test_name) {
  if (enable_test_screenshots_) {
    SynchronizeAnimationLoadWithCompositor();
    screenshot_tester_.Run(test_name);
  }
}

void ScreenshotTestingMixin::IgnoreArea(const SkIRect& area) {
  screenshot_tester_.IgnoreArea(area);
}

// Current implementation is a mockup.
// It simply waits for 5 seconds, assuming that this time is enough for
// animation to load completely.
// TODO(elizavetai): Replace this temporary hack with getting a
// valid notification from compositor.
void ScreenshotTestingMixin::SynchronizeAnimationLoadWithCompositor() {
  base::RunLoop waiter;
  animation_waiter_quitter_ = waiter.QuitClosure();
  timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(2), this,
               &ScreenshotTestingMixin::HandleAnimationLoad);
  waiter.Run();
}

void ScreenshotTestingMixin::HandleAnimationLoad() {
  timer_.Stop();
  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           animation_waiter_quitter_);
}

}  // namespace chromeos
