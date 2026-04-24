// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/decorators_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"

namespace performance_manager {

using PageLiveStateDecoratorHelperTabsBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(PageLiveStateDecoratorHelperTabsBrowserTest,
                       IsActiveTab) {
  // Create a tab, it's associated PageNode should be the active one.
  chrome::AddTabAt(browser(), GURL("http://foo/1"), 0, true);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, true);

  // Create another tab. This immediately makes it the active tab.
  chrome::AddTabAt(browser(), GURL("http://foo/2"), 0, true);
  content::WebContents* other_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_NE(contents, other_contents);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, false);
  testing::TestPageNodeProperty(
      other_contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, true);

  // Reactivate the initial tab, the previously active tab is now inactive.
  browser()->tab_strip_model()->ActivateTabAt(1);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, true);
  testing::TestPageNodeProperty(
      other_contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, false);

  // Deleting a tab automatically makes another one active.
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(1);
  testing::TestPageNodeProperty(
      other_contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsActiveTab, true);
}

IN_PROC_BROWSER_TEST_F(PageLiveStateDecoratorHelperTabsBrowserTest,
                       IsPinnedTab) {
  // Create a tab, it's associated PageNode should be the active one.
  chrome::AddTabAt(browser(), GURL("http://foo/1"), 0, true);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, false);

  browser()->tab_strip_model()->SetTabPinned(0, true);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, true);

  browser()->tab_strip_model()->SetTabPinned(0, false);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, false);
}

IN_PROC_BROWSER_TEST_F(PageLiveStateDecoratorHelperTabsBrowserTest,
                       ReplacePinnedTab) {
  chrome::AddTabAt(browser(), GURL("http://foo/1"), 0, true);
  content::WebContents* contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  // Pin tab. Check status.
  browser()->tab_strip_model()->SetTabPinned(0, true);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, true);

  // Replace with new contents.
  browser()->tab_strip_model()->DiscardWebContentsAt(
      0, content::WebContents::Create(
             content::WebContents::CreateParams(browser()->profile())));

  // Check pinned status of replaced contents.
  contents = browser()->tab_strip_model()->GetWebContentsAt(0);
  testing::TestPageNodeProperty(
      contents, &PageLiveStateDecorator::Data::GetOrCreateForPageNode,
      &PageLiveStateDecorator::Data::IsPinnedTab, true);
}

}  // namespace performance_manager
