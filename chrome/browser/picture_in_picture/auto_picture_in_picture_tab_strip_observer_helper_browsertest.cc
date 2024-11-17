// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/mock_callback.h"
#include "chrome/browser/picture_in_picture/auto_picture_in_picture_tab_strip_observer_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace {

using testing::_;

class AutoPictureInPictureTabStripObserverHelperBrowserTest
    : public InProcessBrowserTest {
 protected:
  void OpenNewForegroundTab(Browser* browser) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }

  void OpenNewBackgroundTab(Browser* browser) {
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser, GURL(url::kAboutBlankURL),
        WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  }
};

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabStripObserverHelperBrowserTest,
                       TriggersOnTabActivationChanged) {
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::MockCallback<
      AutoPictureInPictureTabStripObserverHelper::ActivatedChangedCallback>
      callback;
  AutoPictureInPictureTabStripObserverHelper helper(original_web_contents,
                                                    callback.Get());
  helper.StartObserving();
  EXPECT_EQ(helper.GetActiveWebContents(), original_web_contents);

  // Opening and switching to a new tab should trigger the callback with
  // `is_tab_activated` set to false.
  EXPECT_CALL(callback, Run(false));
  OpenNewForegroundTab(browser());
  auto* second_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(helper.GetActiveWebContents(), second_web_contents);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Switching back to the original tab should trigger the callback with
  // `is_tab_activated` set to true.
  EXPECT_CALL(callback, Run(true));
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Switching away again should trigger again.
  EXPECT_CALL(callback, Run(false));
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(second_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Opening and switching to a new tab here should not trigger anything since
  // we're still not activated.
  EXPECT_CALL(callback, Run(_)).Times(0);
  OpenNewForegroundTab(browser());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Then switching back to the original tab should trigger again.
  EXPECT_CALL(callback, Run(true));
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Telling the helper to stop observing should prevent it from triggering on
  // new changes.
  EXPECT_CALL(callback, Run(_)).Times(0);
  helper.StopObserving();
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(second_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Telling it to start observing again should make it start triggering on
  // changes again.
  EXPECT_CALL(callback, Run(true));
  helper.StartObserving();
  browser()->tab_strip_model()->ActivateTabAt(
      browser()->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabStripObserverHelperBrowserTest,
                       ObservesCorrectTabStrip) {
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::MockCallback<
      AutoPictureInPictureTabStripObserverHelper::ActivatedChangedCallback>
      callback;
  AutoPictureInPictureTabStripObserverHelper helper(original_web_contents,
                                                    callback.Get());
  helper.StartObserving();

  // Opening a new tab in the background should not trigger the callback.
  EXPECT_CALL(callback, Run(_)).Times(0);
  OpenNewBackgroundTab(browser());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Opening a new window should not trigger the callback.
  EXPECT_CALL(callback, Run(_)).Times(0);
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(second_browser);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Switch to the second tab, which should trigger the callback.
  EXPECT_CALL(callback, Run(false));
  browser()->tab_strip_model()->ActivateTabAt(1);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Moving the original tab to the second window should make the helper start
  // watching the second browser's tabstrip.
  //
  // First, move it to the new window in the foreground, which should trigger
  // the callback.
  EXPECT_CALL(callback, Run(true));
  auto* second_browser_initial_web_contents =
      second_browser->tab_strip_model()->GetActiveWebContents();
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(
          browser()->tab_strip_model()->GetIndexOfWebContents(
              original_web_contents));
  second_browser->tab_strip_model()->AppendTab(std::move(detached_tab),
                                               /*foreground=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Then backgrounding it should trigger the callback.
  EXPECT_CALL(callback, Run(false));
  second_browser->tab_strip_model()->ActivateTabAt(
      second_browser->tab_strip_model()->GetIndexOfWebContents(
          second_browser_initial_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);

  // And the same for foregrounding it.
  EXPECT_CALL(callback, Run(true));
  second_browser->tab_strip_model()->ActivateTabAt(
      second_browser->tab_strip_model()->GetIndexOfWebContents(
          original_web_contents));
  testing::Mock::VerifyAndClearExpectations(&callback);
}

IN_PROC_BROWSER_TEST_F(AutoPictureInPictureTabStripObserverHelperBrowserTest,
                       DoesNotTriggerWhenMovedBetweenTabStrips) {
  auto* original_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::MockCallback<
      AutoPictureInPictureTabStripObserverHelper::ActivatedChangedCallback>
      callback;
  AutoPictureInPictureTabStripObserverHelper helper(original_web_contents,
                                                    callback.Get());
  helper.StartObserving();

  // Opening a new tab in the background should not trigger the callback.
  EXPECT_CALL(callback, Run(_)).Times(0);
  OpenNewBackgroundTab(browser());
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Opening a new window should not trigger the callback.
  EXPECT_CALL(callback, Run(_)).Times(0);
  Browser* second_browser = CreateBrowser(browser()->profile());
  ASSERT_TRUE(second_browser);
  testing::Mock::VerifyAndClearExpectations(&callback);

  // Now move the tab into the second window while keeping it in the foreground.
  // This should not trigger the callback.
  EXPECT_CALL(callback, Run(_)).Times(0);
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser()->tab_strip_model()->DetachTabAtForInsertion(
          browser()->tab_strip_model()->GetIndexOfWebContents(
              original_web_contents));
  second_browser->tab_strip_model()->AppendTab(std::move(detached_tab),
                                               /*foreground=*/true);
  testing::Mock::VerifyAndClearExpectations(&callback);
}

}  // namespace
