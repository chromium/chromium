// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_

#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class HeadlessModeBrowserTest : public InProcessBrowserTest {
 public:
  static constexpr char kHeadlessSwitchValue[] = "new";

  HeadlessModeBrowserTest() = default;

  HeadlessModeBrowserTest(const HeadlessModeBrowserTest&) = delete;
  HeadlessModeBrowserTest& operator=(const HeadlessModeBrowserTest&) = delete;

  ~HeadlessModeBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
};

enum StartWindowMode {
  kStartWindowNormal,
  kStartWindowMaximized,
  kStartWindowFullscreen,
};

class HeadlessModeBrowserTestWithStartWindowMode
    : public HeadlessModeBrowserTest,
      public testing::WithParamInterface<StartWindowMode> {
 public:
  HeadlessModeBrowserTestWithStartWindowMode() = default;

  HeadlessModeBrowserTestWithStartWindowMode(
      const HeadlessModeBrowserTestWithStartWindowMode&) = delete;
  HeadlessModeBrowserTestWithStartWindowMode& operator=(
      const HeadlessModeBrowserTestWithStartWindowMode&) = delete;

  ~HeadlessModeBrowserTestWithStartWindowMode() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  StartWindowMode start_window_mode() const { return GetParam(); }
};

// Toggles browser fullscreen mode synchronously.
void ToggleFullscreenModeSync(Browser* browser);

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
