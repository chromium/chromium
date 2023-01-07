// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/accessibility_labels_bubble_model.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// A test class for the accessibility labels bubble.
class AccessibilityLabelsBubbleModelTest : public InProcessBrowserTest {
 public:
  AccessibilityLabelsBubbleModelTest() = default;
  ~AccessibilityLabelsBubbleModelTest() override = default;
  AccessibilityLabelsBubbleModelTest(
      const AccessibilityLabelsBubbleModelTest&) = delete;
  AccessibilityLabelsBubbleModelTest& operator=(
      const AccessibilityLabelsBubbleModelTest&) = delete;

  std::unique_ptr<AccessibilityLabelsBubbleModel> CreateConfirmBubble() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto model = std::make_unique<AccessibilityLabelsBubbleModel>(
        browser()->profile(), web_contents, /*enable_always=*/true);
    return model;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBubbleModelTest, ConfirmSetsPref) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);
  std::unique_ptr<AccessibilityLabelsBubbleModel> model = CreateConfirmBubble();
  model->Accept();
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityImageLabelsEnabled));
}

IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBubbleModelTest,
                       CancelDoesNotSetPref) {
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kAccessibilityImageLabelsEnabled, false);
  std::unique_ptr<AccessibilityLabelsBubbleModel> model = CreateConfirmBubble();
  model->Cancel();
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kAccessibilityImageLabelsEnabled));
}

IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBubbleModelTest, OpenHelpPage) {
  std::unique_ptr<AccessibilityLabelsBubbleModel> model = CreateConfirmBubble();
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  model->OpenHelpPage();
  content::WebContents* web_contents = waiter.Wait();
  EXPECT_EQ(web_contents->GetBrowserContext(), browser()->profile());
  EXPECT_EQ(web_contents->GetVisibleURL(), model->GetHelpPageURL());
}

// Tests that closing the tab with WebContents that was used to construct
// the AccessibilityLabelsBubbleModel does not cause any problems when
// opening the Help page.
// This is a regression test for crbug.com/1212500.
// Note that we do not need to test what happens when the whole browser
// closes, because when the last tab in a window closes it will close the
// bubble widget too.
IN_PROC_BROWSER_TEST_F(AccessibilityLabelsBubbleModelTest,
                       OpenHelpPageAfterWebContentsClosed) {
  // Open a new tab so the whole browser does not close once we close
  // the tab via WebContents::Close() below.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("data:text/html,<p>kittens!</p>"),
                            ui::PAGE_TRANSITION_TYPED));
  std::unique_ptr<AccessibilityLabelsBubbleModel> model = CreateConfirmBubble();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  model->OpenHelpPage();
  content::WebContents* web_contents = waiter.Wait();
  EXPECT_EQ(web_contents->GetBrowserContext(), browser()->profile());
  EXPECT_EQ(web_contents->GetVisibleURL(), model->GetHelpPageURL());
}
