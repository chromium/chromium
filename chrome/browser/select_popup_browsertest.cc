// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "third_party/blink/public/common/features.h"

class SelectPopupBrowsertest : public InProcessBrowserTest {
 public:
  SelectPopupBrowsertest() {
    feature_list_.InitAndEnableFeature(blink::features::kCSSPseudoOpenClosed);
  }

  void TestBody() override {}
  void RunTestOnMainThread() override {}

 private:
  base::test::ScopedFeatureList feature_list_;
};

// <select> popups should not be created for background tabs.
// http://crbug.com/1521345
IN_PROC_BROWSER_TEST_F(SelectPopupBrowsertest, SelectPopupHiddenTab) {
  // Open a tab with a select element
  content::RenderFrameHost* first_rfh = ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,<select><option>one</option></select>"));
  ASSERT_TRUE(first_rfh);

  // Open a second tab to hide the first one
  content::RenderFrameHost* second_rfh =
      ui_test_utils::NavigateToURLWithDisposition(
          browser(), GURL("data:text/html,<div>new tab</div>"),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(second_rfh);

  // The new tab should be open and the first one should be hidden
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  content::WebContents* first_web_contents =
      content::WebContents::FromRenderFrameHost(first_rfh);
  content::TitleWatcher title_watcher(first_web_contents, u"foo");

  // Open the select popup from the first tab
  first_rfh->ExecuteJavaScriptWithUserGestureForTests(
      u"document.querySelector('select').showPicker();"
      u"document.title = 'foo';",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);

  // Wait for the popup to open
  ASSERT_EQ(title_watcher.WaitAndGetTitle(), u"foo");

  // Check to see if a popup was opened
  content::TitleWatcher finished_title_watcher(first_web_contents, u"pass");
  finished_title_watcher.AlsoWaitForTitle(u"fail");
  first_rfh->ExecuteJavaScriptWithUserGestureForTests(
      u"document.title = document.querySelector('select').matches(':open')"
      u" ? 'fail' : 'pass';",
      base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);
  ASSERT_EQ(finished_title_watcher.WaitAndGetTitle(), u"pass");
}
