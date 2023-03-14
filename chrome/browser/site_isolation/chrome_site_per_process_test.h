// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_
#define CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_

#include "base/feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"

namespace base {
class CommandLine;
}  // namespace base

class ChromeSitePerProcessTest : public InProcessBrowserTest {
 public:
  ChromeSitePerProcessTest();

  ChromeSitePerProcessTest(const ChromeSitePerProcessTest&) = delete;
  ChromeSitePerProcessTest& operator=(const ChromeSitePerProcessTest&) = delete;

  ~ChromeSitePerProcessTest() override;

  void SetUpCommandLine(base::CommandLine* command_line) override;

  void SetUpOnMainThread() override;

  base::test::ScopedFeatureList* feature_list() { return &feature_list_; }

 private:
  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_BROWSER_SITE_ISOLATION_CHROME_SITE_PER_PROCESS_TEST_H_
