// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/download/download_started_animation.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class DownloadStartedAnimationTest : public InProcessBrowserTest {
 public:
  DownloadStartedAnimationTest() {
  }

  DownloadStartedAnimationTest(const DownloadStartedAnimationTest&) = delete;
  DownloadStartedAnimationTest& operator=(const DownloadStartedAnimationTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(DownloadStartedAnimationTest,
                       InstantiateAndImmediatelyClose) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DownloadStartedAnimation::Show(web_contents);
  chrome::CloseWindow(browser());
}
