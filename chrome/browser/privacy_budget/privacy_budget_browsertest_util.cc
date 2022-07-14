// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_budget/privacy_budget_browsertest_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"

PrivacyBudgetBrowserTestBaseWithTestRecorder::
    PrivacyBudgetBrowserTestBaseWithTestRecorder() = default;
PrivacyBudgetBrowserTestBaseWithTestRecorder::
    ~PrivacyBudgetBrowserTestBaseWithTestRecorder() = default;

void PrivacyBudgetBrowserTestBaseWithTestRecorder::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  // Do an initial empty navigation then create the recorder to make sure we
  // start on a clean slate. This clears the platform differences in between
  // Android and Desktop.
  content::NavigateToURLBlockUntilNavigationsComplete(web_contents(),
                                                      GURL("about:blank"), 1);

  // Ensure that the actively sampled surfaces reported at browser startup go
  // through before we set up the test recorder.
  content::RunAllTasksUntilIdle();
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
}

content::WebContents*
PrivacyBudgetBrowserTestBaseWithTestRecorder::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}
