// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cros_apps/api/cros_apps_api_browsertest_base.h"

#include "chrome/browser/ui/browser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using CrosAppsChromeosApiBrowserTest = CrosAppsApiBrowserTestBase;

IN_PROC_BROWSER_TEST_F(CrosAppsChromeosApiBrowserTest, ChromeosExists) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  EXPECT_EQ(true, content::EvalJs(web_contents,
                                  "typeof window.chromeos !== 'undefined';"));
}
