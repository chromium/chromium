// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_
#define CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_

#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/ukm/test_ukm_recorder.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace content {
class WebContents;
}  // namespace content

class PrivacyBudgetBrowserTestBaseWithTestRecorder
    : public PlatformBrowserTest {
 public:
  PrivacyBudgetBrowserTestBaseWithTestRecorder();
  ~PrivacyBudgetBrowserTestBaseWithTestRecorder() override;
  void SetUpOnMainThread() override;

  ukm::TestUkmRecorder& recorder() { return *ukm_recorder_; }

  content::WebContents* web_contents();

 private:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
};

#endif  // CHROME_BROWSER_PRIVACY_BUDGET_PRIVACY_BUDGET_BROWSERTEST_UTIL_H_
