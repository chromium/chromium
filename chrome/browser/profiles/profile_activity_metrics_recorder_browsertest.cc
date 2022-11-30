// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_activity_metrics_recorder.h"

#include "base/metrics/user_metrics.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

// Checks that base::RecordAction() can be called without issue after
// BrowserProcess shutdown. The test fixture destructor gets called
// after shutdown, so we call it there.
//
// Regression test for crbug.com/1094407 where these calls lead to a
// use-after-free.
class ProfileActivityMetricsRecorderLogAfterQuitTest
    : public InProcessBrowserTest {
 public:
  ~ProfileActivityMetricsRecorderLogAfterQuitTest() override {
    base::RecordAction(base::UserMetricsAction("Test_Action"));
  }
};

IN_PROC_BROWSER_TEST_F(ProfileActivityMetricsRecorderLogAfterQuitTest, Test) {}
