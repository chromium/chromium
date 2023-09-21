// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/lacros/cros_apps/cros_apps_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class CrosAppsTabHelperBrowserTest : public InProcessBrowserTest {
 public:
  CrosAppsTabHelperBrowserTest() {
    scoped_feature_list_.InitWithFeatures({chromeos::features::kBlinkExtension},
                                          {});
  }
  CrosAppsTabHelperBrowserTest(const CrosAppsTabHelperBrowserTest&) = delete;
  CrosAppsTabHelperBrowserTest& operator=(const CrosAppsTabHelperBrowserTest&) =
      delete;
  ~CrosAppsTabHelperBrowserTest() override = default;

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(CrosAppsTabHelperBrowserTest, EnableFeatureInMainFrame) {
  ASSERT_TRUE(chromeos::features::IsBlinkExtensionEnabled());

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  NavigateToURLBlockUntilNavigationsComplete(
      web_contents, embedded_test_server()->GetURL("/iframe.html"),
      /*num_of_navigations*/ 1);

  EXPECT_EQ(true, IsIdentifierDefined(web_contents, "window.chromeos"));
  auto* child_frame =
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(child_frame);
  EXPECT_EQ(false, IsIdentifierDefined(child_frame, "window.chromeos"));
}
