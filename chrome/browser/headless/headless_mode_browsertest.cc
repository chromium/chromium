// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/headless/headless_mode_util.h"

#include <memory>
#include <string>

#include "base/command_line.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/switches.h"

#if defined(OS_LINUX)
#include "ui/ozone/public/ozone_platform.h"
#endif  // defined(OS_LINUX)

namespace {
const char kChrome[] = "chrome";
}  // namespace

class HeadlessModeBrowserTest : public InProcessBrowserTest {
 public:
  HeadlessModeBrowserTest() = default;

  HeadlessModeBrowserTest(const HeadlessModeBrowserTest&) = delete;
  HeadlessModeBrowserTest& operator=(const HeadlessModeBrowserTest&) = delete;

  ~HeadlessModeBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);

    command_line->AppendSwitchASCII(switches::kHeadless, kChrome);
    headless::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(headless::IsChromeNativeHeadless());
  }

 private:
};

#if defined(OS_LINUX)
IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, OzonePlatformHeadless) {
  // On Linux, the Native Headless Chrome uses Ozone/Headless.
  ASSERT_NE(ui::OzonePlatform::GetInstance(), nullptr);
  EXPECT_EQ(ui::OzonePlatform::GetPlatformNameForTest(), "headless");
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(HeadlessModeBrowserTest, BrowserDesktopWindowHidden) {
  // On Windows, the Native Headless Chrome browser window exists but is hidden.
  EXPECT_FALSE(browser()->window()->IsVisible());
}
#endif  // defined(OS_WIN)
