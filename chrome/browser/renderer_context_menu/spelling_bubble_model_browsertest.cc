// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_context_menu/spelling_bubble_model.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/spellcheck/browser/pref_names.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// A test class for the spelling confirmation bubble.
class SpellingBubbleModelTest : public InProcessBrowserTest {
 public:
  SpellingBubbleModelTest() = default;
  ~SpellingBubbleModelTest() override = default;
  SpellingBubbleModelTest(const SpellingBubbleModelTest&) = delete;
  SpellingBubbleModelTest& operator=(const SpellingBubbleModelTest&) = delete;

  std::unique_ptr<SpellingBubbleModel> CreateSpellingBubble() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    auto model = std::make_unique<SpellingBubbleModel>(browser()->profile(),
                                                       web_contents);
    return model;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SpellingBubbleModelTest, ConfirmSetsPrefs) {
  browser()->profile()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, false);
  browser()->profile()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckEnable, false);
  std::unique_ptr<SpellingBubbleModel> model = CreateSpellingBubble();
  model->Accept();
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService));
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckEnable));
}

IN_PROC_BROWSER_TEST_F(SpellingBubbleModelTest, CancelSetsPref) {
  browser()->profile()->GetPrefs()->SetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService, true);
  std::unique_ptr<SpellingBubbleModel> model = CreateSpellingBubble();
  model->Cancel();
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      spellcheck::prefs::kSpellCheckUseSpellingService));
}

IN_PROC_BROWSER_TEST_F(SpellingBubbleModelTest, OpenHelpPage) {
  std::unique_ptr<SpellingBubbleModel> model = CreateSpellingBubble();
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  model->OpenHelpPage();
  content::WebContents* web_contents = waiter.Wait();
  EXPECT_EQ(web_contents->GetBrowserContext(), browser()->profile());
  EXPECT_EQ(web_contents->GetVisibleURL(), model->GetHelpPageURL());
}

// Tests that closing the tab with WebContents that was used to construct the
// SpellingBubbleModel does not cause any problems when opening the Help page.
// This is a regression test for crbug.com/1212498.
// Note that we do not need to test what happens when the whole browser
// closes, because when the last tab in a window closes it will close the
// bubble widget too.
IN_PROC_BROWSER_TEST_F(SpellingBubbleModelTest,
                       OpenHelpPageAfterWebContentsClosed) {
  // Open a new tab so the whole browser does not close once we close
  // the tab via WebContents::Close() below.
  ASSERT_TRUE(AddTabAtIndex(0, GURL("data:text/html,<p>puppies!</p>"),
                            ui::PAGE_TRANSITION_TYPED));
  std::unique_ptr<SpellingBubbleModel> model = CreateSpellingBubble();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();
  ui_test_utils::AllBrowserTabAddedWaiter waiter;
  model->OpenHelpPage();
  content::WebContents* web_contents = waiter.Wait();
  EXPECT_EQ(web_contents->GetBrowserContext(), browser()->profile());
  EXPECT_EQ(web_contents->GetVisibleURL(), model->GetHelpPageURL());
}
