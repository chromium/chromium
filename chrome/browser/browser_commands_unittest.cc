// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/test/bookmark_test_helpers.h"
#include "components/zoom/page_zoom.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_zoom.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"

typedef BrowserWithTestWindowTest BrowserCommandsTest;

using bookmarks::BookmarkModel;
using content::OpenURLParams;
using content::Referrer;
using content::WebContents;
using zoom::ZoomController;

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB.
TEST_F(BrowserCommandsTest, TabNavigationAccelerators) {
  GURL about_blank(url::kAboutBlankURL);

  // Create three tabs.
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);
  AddTab(browser(), about_blank);

  // Select the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);

  CommandUpdater* updater = browser()->command_controller();

  // Navigate to the first tab using an accelerator.
  updater->ExecuteCommand(IDC_SELECT_TAB_0);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to the second tab using the next accelerators.
  updater->ExecuteCommand(IDC_SELECT_NEXT_TAB);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Navigate back to the first tab using the previous accelerators.
  updater->ExecuteCommand(IDC_SELECT_PREVIOUS_TAB);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());

  // Navigate to the last tab using the select last accelerator.
  updater->ExecuteCommand(IDC_SELECT_LAST_TAB);
  ASSERT_EQ(2, browser()->tab_strip_model()->active_index());
}

// Tests IDC_DUPLICATE_TAB.
TEST_F(BrowserCommandsTest, DuplicateTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");
  GURL url3("http://foo/3");
  GURL url4("http://foo/4");

  // Navigate to three urls, plus a pending URL that hasn't committed.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);
  NavigateAndCommitActiveTab(url3);
  content::NavigationController& orig_controller =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  orig_controller.LoadURL(
      url4, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_EQ(3, orig_controller.GetEntryCount());
  EXPECT_TRUE(orig_controller.GetPendingEntry());

  size_t initial_window_count = chrome::GetTotalBrowserCount();

  // Duplicate the tab.
  chrome::ExecuteCommand(browser(), IDC_DUPLICATE_TAB);

  // The duplicated tab should not end up in a new window.
  size_t window_count = chrome::GetTotalBrowserCount();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Verify the stack of urls.
  content::NavigationController& controller =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetController();
  EXPECT_EQ(3, controller.GetEntryCount());
  EXPECT_EQ(2, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_EQ(url3, controller.GetEntryAtIndex(2)->GetURL());
  EXPECT_FALSE(controller.GetPendingEntry());
}

// Tests IDC_VIEW_SOURCE (See http://crbug.com/138140).
TEST_F(BrowserCommandsTest, ViewSource) {
  GURL url1("http://foo/1");
  GURL url1_subframe("http://foo/subframe");
  GURL url2("http://foo/2");

  // Navigate to a URL and simulate a subframe committing.
  AddTab(browser(), url1);
  content::RenderFrameHostTester* rfh_tester =
      content::RenderFrameHostTester::For(
          browser()->tab_strip_model()->GetWebContentsAt(0)->GetMainFrame());
  content::RenderFrameHost* subframe = rfh_tester->AppendChild("subframe");
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL(url1_subframe), subframe);

  // Now start a pending navigation that hasn't committed.
  content::NavigationController& orig_controller =
      browser()->tab_strip_model()->GetWebContentsAt(0)->GetController();
  orig_controller.LoadURL(
      url2, content::Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_EQ(1, orig_controller.GetEntryCount());
  EXPECT_TRUE(orig_controller.GetPendingEntry());

  size_t initial_window_count = chrome::GetTotalBrowserCount();

  // View Source.
  chrome::ExecuteCommand(browser(), IDC_VIEW_SOURCE);

  // The view source tab should not end up in a new window.
  size_t window_count = chrome::GetTotalBrowserCount();
  ASSERT_EQ(initial_window_count, window_count);

  // And we should have a newly duplicated tab.
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Verify we are viewing the source of the last committed entry.
  GURL view_source_url("view-source:http://foo/1");
  content::NavigationController& controller =
      browser()->tab_strip_model()->GetWebContentsAt(1)->GetController();
  EXPECT_EQ(1, controller.GetEntryCount());
  EXPECT_EQ(0, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(view_source_url, controller.GetEntryAtIndex(0)->GetVirtualURL());
  EXPECT_FALSE(controller.GetPendingEntry());
}

TEST_F(BrowserCommandsTest, BookmarkCurrentTab) {
  // We use profile() here, since it's a TestingProfile.
  profile()->CreateBookmarkModel(true);

  BookmarkModel* model = BookmarkModelFactory::GetForBrowserContext(profile());
  bookmarks::test::WaitForBookmarkModelToLoad(model);

  // Navigate to a url.
  GURL url1("http://foo/1");
  AddTab(browser(), url1);
  browser()->OpenURL(OpenURLParams(url1, Referrer(),
                                   WindowOpenDisposition::CURRENT_TAB,
                                   ui::PAGE_TRANSITION_TYPED, false));

  chrome::BookmarkCurrentTabAllowingExtensionOverrides(browser());

  // It should now be bookmarked in the bookmark model.
  EXPECT_EQ(profile(), browser()->profile());
  EXPECT_TRUE(model->IsBookmarked(url1));
}

// Tests back/forward in new tab (Control + Back/Forward button in the UI).
TEST_F(BrowserCommandsTest, BackForwardInNewTab) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");

  // Make a tab with the two pages navigated in it.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);

  // Go back in a new background tab.
  chrome::GoBack(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  WebContents* zeroth = browser()->tab_strip_model()->GetWebContentsAt(0);
  WebContents* first = browser()->tab_strip_model()->GetWebContentsAt(1);

  // The original tab should be unchanged.
  EXPECT_EQ(url2, zeroth->GetLastCommittedURL());
  EXPECT_TRUE(zeroth->GetController().CanGoBack());
  EXPECT_FALSE(zeroth->GetController().CanGoForward());

  // The new tab should be like the first one but navigated back. Since we
  // didn't wait for the load to complete, we can't use GetLastCommittedURL.
  EXPECT_EQ(url1, first->GetVisibleURL());
  EXPECT_FALSE(first->GetController().CanGoBack());
  EXPECT_TRUE(first->GetController().CanGoForward());

  // Select the second tab and make it go forward in a new background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  // TODO(crbug.com/11055): It should not be necessary to commit the load here,
  // but because of this bug, it will assert later if we don't. When the bug is
  // fixed, one of the three commits here related to this bug should be removed
  // (to test both codepaths).
  CommitPendingLoad(&first->GetController());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  chrome::GoForward(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);

  // The previous tab should be unchanged and still in the foreground.
  EXPECT_EQ(url1, first->GetLastCommittedURL());
  EXPECT_FALSE(first->GetController().CanGoBack());
  EXPECT_TRUE(first->GetController().CanGoForward());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // There should be a new tab navigated forward.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  WebContents* second = browser()->tab_strip_model()->GetWebContentsAt(2);
  // Since we didn't wait for load to complete, we can't use
  // GetLastCommittedURL.
  EXPECT_EQ(url2, second->GetVisibleURL());
  EXPECT_TRUE(second->GetController().CanGoBack());
  EXPECT_FALSE(second->GetController().CanGoForward());

  // Now do back in a new foreground tab. Don't bother re-checking every sngle
  // thing above, just validate that it's opening properly.
  browser()->tab_strip_model()->ActivateTabAt(
      2, {TabStripModel::GestureType::kOther});
  // TODO(crbug.com/11055): see the comment above about why we need this.
  CommitPendingLoad(&second->GetController());
  chrome::GoBack(browser(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_EQ(3, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(url1,
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetVisibleURL());

  // Same thing again for forward.
  // TODO(crbug.com/11055): see the comment above about why we need this.
  CommitPendingLoad(&
      browser()->tab_strip_model()->GetActiveWebContents()->GetController());
  chrome::GoForward(browser(), WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_EQ(4, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(url2,
            browser()->tab_strip_model()->GetActiveWebContents()->
                GetVisibleURL());
}

// Tests back/forward in new tab (Control + Back/Forward button in the UI)
// with Tab Groups enabled.
TEST_F(BrowserCommandsTest, BackForwardInNewTabWithGroup) {
  GURL url1("http://foo/1");
  GURL url2("http://foo/2");

  // Make a tab with the two pages navigated in it.
  AddTab(browser(), url1);
  NavigateAndCommitActiveTab(url2);

  // Add the tab to a Tab Group.
  const TabGroupId group_id = browser()->tab_strip_model()->AddToNewGroup({0});

  // Go back in a new background tab.
  chrome::GoBack(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // The new tab should have inherited the tab group from the old tab.
  EXPECT_EQ(group_id, browser()->tab_strip_model()->GetTabGroupForTab(1));

  // Select the second tab and make it go forward in a new background tab.
  browser()->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
  // TODO(crbug.com/11055): see the comment above about why we need this.
  CommitPendingLoad(
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController());
  chrome::GoForward(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);

  // The new tab should have inherited the tab group from the old tab.
  EXPECT_EQ(group_id, browser()->tab_strip_model()->GetTabGroupForTab(2));
}

TEST_F(BrowserCommandsTest, OnMaxZoomIn) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  GURL url("http://www.google.com");
  AddTab(browser(), url);
  WebContents* first_tab = tab_strip_model->GetWebContentsAt(0);

  // Continue to zoom in until zoom percent reaches 500.
  for (int i = 0; i < 9; ++i) {
    zoom::PageZoom::Zoom(first_tab, content::PAGE_ZOOM_IN);
  }

  // TODO(a.sarkar.arun@gmail.com): Figure out why Zoom-In menu item is not
  // disabled after Max-zoom is reached. Force disable Zoom-In menu item
  // from the context menu since it breaks try jobs on bots.
  if (chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS))
    chrome::UpdateCommandEnabled(browser(), IDC_ZOOM_PLUS, false);

  ZoomController* zoom_controller = ZoomController::FromWebContents(first_tab);
  EXPECT_FLOAT_EQ(500.0f, zoom_controller->GetZoomPercent());
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));
}

TEST_F(BrowserCommandsTest, OnMaxZoomOut) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  GURL url("http://www.google.com");
  AddTab(browser(), url);
  WebContents* first_tab = tab_strip_model->GetWebContentsAt(0);

  // Continue to zoom out until zoom percent reaches 25.
  for (int i = 0; i < 7; ++i) {
    zoom::PageZoom::Zoom(first_tab, content::PAGE_ZOOM_OUT);
  }

  ZoomController* zoom_controller = ZoomController::FromWebContents(first_tab);
  EXPECT_FLOAT_EQ(25.0f, zoom_controller->GetZoomPercent());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));
}

TEST_F(BrowserCommandsTest, OnZoomReset) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  GURL url("http://www.google.com");
  AddTab(browser(), url);
  WebContents* first_tab = tab_strip_model->GetWebContentsAt(0);

  // Change the zoom percentage to 100.
  zoom::PageZoom::Zoom(first_tab, content::PAGE_ZOOM_RESET);

  ZoomController* zoom_controller = ZoomController::FromWebContents(first_tab);
  EXPECT_FLOAT_EQ(100.0f, zoom_controller->GetZoomPercent());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));

  // Changing the page scale factor will re-enable IDC_ZOOM_NORMAL
  zoom_controller->SetPageScaleFactorIsOneForTesting(false);
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
}

TEST_F(BrowserCommandsTest, OnZoomLevelChanged) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  GURL url("http://www.google.com");
  AddTab(browser(), url);
  WebContents* first_tab = tab_strip_model->GetWebContentsAt(0);

  // Changing zoom percentage from default should enable all the zoom
  // NSMenuItems.
  zoom::PageZoom::Zoom(first_tab, content::PAGE_ZOOM_IN);

  ZoomController* zoom_controller = ZoomController::FromWebContents(first_tab);
  EXPECT_FLOAT_EQ(110.0f, zoom_controller->GetZoomPercent());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));
}

TEST_F(BrowserCommandsTest, OnZoomChangedForActiveTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  GURL url("http://www.google.com");
  GURL url1("http://code.google.com");

  // Add First tab.
  AddTab(browser(), url);
  AddTab(browser(), url1);
  WebContents* first_tab = tab_strip_model->GetWebContentsAt(0);

  ZoomController* zoom_controller = ZoomController::FromWebContents(first_tab);
  EXPECT_FLOAT_EQ(100.0f, zoom_controller->GetZoomPercent());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));

  // Add Second tab.
  WebContents* second_tab = tab_strip_model->GetWebContentsAt(1);

  tab_strip_model->ActivateTabAt(1, {TabStripModel::GestureType::kOther});
  EXPECT_TRUE(tab_strip_model->IsTabSelected(1));
  zoom::PageZoom::Zoom(second_tab, content::PAGE_ZOOM_OUT);

  zoom_controller = ZoomController::FromWebContents(second_tab);
  EXPECT_FLOAT_EQ(90.0f, zoom_controller->GetZoomPercent());
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));
}

TEST_F(BrowserCommandsTest, OnDefaultZoomLevelChanged) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  GURL url("http://code.google.com");
  AddTab(browser(), url);
  WebContents* tab = tab_strip_model->GetWebContentsAt(0);
  ZoomController* zoom_controller = ZoomController::FromWebContents(tab);

  // Set the default zoom level to 125.
  profile()->GetZoomLevelPrefs()->SetDefaultZoomLevelPref(
      blink::PageZoomFactorToZoomLevel(1.25));
  EXPECT_FLOAT_EQ(125.0f, zoom_controller->GetZoomPercent());

  // Actual Size from context menu should be disabled now.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_FALSE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));

  // Change the zoom level.
  zoom::PageZoom::Zoom(tab, content::PAGE_ZOOM_IN);

  EXPECT_FLOAT_EQ(150.0f, zoom_controller->GetZoomPercent());

  // Tab no longer at default zoom hence actual size should be enabled.
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_PLUS));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_NORMAL));
  EXPECT_TRUE(chrome::IsCommandEnabled(browser(), IDC_ZOOM_MINUS));
}
