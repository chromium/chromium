// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_

#include "base/command_line.h"
#include "chrome/test/base/in_process_browser_test.h"

class HeadlessModeBrowserTest : public InProcessBrowserTest {
 public:
  static constexpr char kHeadlessSwitchValue[] = "chrome";

  HeadlessModeBrowserTest() = default;

  HeadlessModeBrowserTest(const HeadlessModeBrowserTest&) = delete;
  HeadlessModeBrowserTest& operator=(const HeadlessModeBrowserTest&) = delete;

  ~HeadlessModeBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;
};

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
