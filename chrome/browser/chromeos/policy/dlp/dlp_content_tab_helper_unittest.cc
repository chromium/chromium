// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_content_tab_helper.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/chromeos/policy/dlp/test/mock_dlp_content_observer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

using testing::_;
using testing::Return;

namespace {
const DlpContentRestrictionSet kEmptyRestrictionSet;
const DlpContentRestrictionSet kNonEmptyRestrictionSet(
    DlpContentRestriction::kScreenshot,
    DlpRulesManager::Level::kBlock);
}  // namespace

class DlpContentTabHelperTest : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    scoped_dlp_content_observer_ =
        std::make_unique<ScopedDlpContentObserverForTesting>(
            &mock_dlp_content_observer_);

    // Initialize browser.
    const Browser::CreateParams params(profile(), /*user_gesture=*/true);
    browser_ = CreateBrowserWithTestWindowForParams(params);
    tab_strip_model_ = browser_->tab_strip_model();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    browser_.reset();

    scoped_dlp_content_observer_.reset();

    ChromeRenderViewHostTestHarness::TearDown();
  }

  DlpContentTabHelper::ScopedIgnoreDlpRulesManager ignore_dlp_rules_manager_{
      DlpContentTabHelper::IgnoreDlpRulesManagerForTesting()};
  MockDlpContentObserver mock_dlp_content_observer_;
  std::unique_ptr<ScopedDlpContentObserverForTesting>
      scoped_dlp_content_observer_;
  TabActivitySimulator tab_activity_simulator_;
  raw_ptr<TabStripModel, DanglingUntriaged> tab_strip_model_;
  std::unique_ptr<Browser> browser_;
};

TEST_F(DlpContentTabHelperTest, NotCreatedForIncognito) {
  const Browser::CreateParams params(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      /*user_gesture=*/true);
  auto browser = CreateBrowserWithTestWindowForParams(params);

  content::WebContents* web_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(
          browser->tab_strip_model(), GURL("https://example.com"));
  EXPECT_EQ(nullptr, DlpContentTabHelper::FromWebContents(web_contents));

  // Close tabs before |browser| is destructed.
  browser->tab_strip_model()->CloseAllTabs();
}

TEST_F(DlpContentTabHelperTest, NotConfidential) {
  GURL kUrl = GURL("https://example.com");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl, kEmptyRestrictionSet);
  EXPECT_CALL(mock_dlp_content_observer_, OnConfidentialityChanged(_, _))
      .Times(0);
  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(0);

  content::WebContents* web_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_, kUrl);
  EXPECT_NE(nullptr, DlpContentTabHelper::FromWebContents(web_contents));

  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(1);
}

TEST_F(DlpContentTabHelperTest, Confidential) {
  GURL kUrl = GURL("https://example.com");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl, kNonEmptyRestrictionSet);
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kNonEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(0);

  content::WebContents* web_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_, kUrl);
  EXPECT_NE(nullptr, DlpContentTabHelper::FromWebContents(web_contents));

  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(1);
}

TEST_F(DlpContentTabHelperTest, VisibilityChanged) {
  GURL kUrl1 = GURL("https://example1.com");
  GURL kUrl2 = GURL("https://example2.com");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl1, kNonEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kUrl2, kEmptyRestrictionSet);
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kNonEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(0);
  content::WebContents* web_contents1 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_,
                                                        kUrl1);
  content::WebContents* web_contents2 =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_,
                                                        kUrl2);
  EXPECT_NE(nullptr, DlpContentTabHelper::FromWebContents(web_contents1));
  EXPECT_NE(nullptr, DlpContentTabHelper::FromWebContents(web_contents2));
  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(1);

  tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, 1);

  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(1);

  tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, 0);

  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(2);
}

TEST_F(DlpContentTabHelperTest, SubFrameNavigation) {
  GURL kNonConfidentialUrl = GURL("https://example.com");
  GURL kConfidentialUrl = GURL("https://google.com");
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      GURL(), kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kNonConfidentialUrl, kEmptyRestrictionSet);
  DlpContentRestrictionSet::SetRestrictionsForURLForTesting(
      kConfidentialUrl, kNonEmptyRestrictionSet);
  EXPECT_CALL(mock_dlp_content_observer_, OnConfidentialityChanged(_, _))
      .Times(0);
  EXPECT_CALL(mock_dlp_content_observer_, OnVisibilityChanged(_)).Times(0);

  // Create WebContents.
  content::WebContents* web_contents =
      tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_,
                                                        kNonConfidentialUrl);
  EXPECT_NE(nullptr, DlpContentTabHelper::FromWebContents(web_contents));

  // Add subframe and navigate to confidential URL.
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kNonEmptyRestrictionSet))
      .Times(1);
  content::RenderFrameHost* subframe =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          kConfidentialUrl, content::RenderFrameHostTester::For(
                                web_contents->GetPrimaryMainFrame())
                                ->AppendChild("child"));

  // Navigate away from confidential URL.
  EXPECT_CALL(mock_dlp_content_observer_,
              OnConfidentialityChanged(_, kEmptyRestrictionSet))
      .Times(1);
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      kNonConfidentialUrl, subframe);

  EXPECT_CALL(mock_dlp_content_observer_, OnWebContentsDestroyed(_)).Times(1);
}

}  // namespace policy
