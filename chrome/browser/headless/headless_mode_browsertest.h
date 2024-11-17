// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
#define CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace content {
class WebContents;
}

namespace headless {

class HeadlessModeBrowserTest : public InProcessBrowserTest {
 public:
  static constexpr char kHeadlessSwitchValue[] = "new";

  HeadlessModeBrowserTest();

  HeadlessModeBrowserTest(const HeadlessModeBrowserTest&) = delete;
  HeadlessModeBrowserTest& operator=(const HeadlessModeBrowserTest&) = delete;

  ~HeadlessModeBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpOnMainThread() override;

 protected:
  virtual bool IsIncognito();

  bool headful_mode() const { return headful_mode_; }

  void AppendHeadlessCommandLineSwitches(base::CommandLine* command_line);

  content::WebContents* GetActiveWebContents();

 private:
  bool headful_mode_ = false;
};

class HeadlessModeBrowserTestWithUserDataDir : public HeadlessModeBrowserTest {
 public:
  HeadlessModeBrowserTestWithUserDataDir() = default;
  ~HeadlessModeBrowserTestWithUserDataDir() override = default;

  base::FilePath GetUserDataDir() const;
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
  ~HeadlessModeBrowserTestWithStartWindowMode() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  StartWindowMode start_window_mode() const { return GetParam(); }
};

// Toggles browser fullscreen mode synchronously.
void ToggleFullscreenModeSync(Browser* browser);

class HeadlessModeBrowserTestWithWindowSize : public HeadlessModeBrowserTest {
 public:
  static constexpr gfx::Size kWindowSize = {4096, 2160};

  HeadlessModeBrowserTestWithWindowSize() = default;
  ~HeadlessModeBrowserTestWithWindowSize() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
};

class HeadlessModeBrowserTestWithWindowSizeAndScale
    : public HeadlessModeBrowserTest {
 public:
  static constexpr gfx::Size kWindowSize = {800, 600};

  HeadlessModeBrowserTestWithWindowSizeAndScale() = default;
  ~HeadlessModeBrowserTestWithWindowSizeAndScale() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override;
};

}  // namespace headless

#endif  // CHROME_BROWSER_HEADLESS_HEADLESS_MODE_BROWSERTEST_H_
