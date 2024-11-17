// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/show_feedback_page.h"

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using ShowFeedbackPageTest = BrowserWithTestWindowTest;

// TODO(crbug.com/1167223): Fix the test for WebUIFeedback.
TEST_F(ShowFeedbackPageTest, DISABLED_UserFeedbackDisallowed) {
  base::HistogramTester histogram_tester;
  std::string unused;
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceBrowserCommand,
                           unused, unused, unused, unused, base::Value::Dict());
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);
  chrome::ShowFeedbackPage(browser(), feedback::kFeedbackSourceBrowserCommand,
                           unused, unused, unused, unused, base::Value::Dict());
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}
