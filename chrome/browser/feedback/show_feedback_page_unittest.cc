// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ShowFeedbackPageTest = BrowserWithTestWindowTest;

TEST_F(ShowFeedbackPageTest, UserFeedbackDisallowed) {
  base::HistogramTester histogram_tester;
  std::string unused;
  chrome::ShowFeedbackPage(browser(), chrome::kFeedbackSourceBrowserCommand,
                           unused, unused, unused, unused);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);
  chrome::ShowFeedbackPage(browser(), chrome::kFeedbackSourceBrowserCommand,
                           unused, unused, unused, unused);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}
