// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_tab_url_provider.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::ElementsAre;

class OptimizationGuideTabUrlProviderTest : public BrowserWithTestWindowTest {
 public:
  OptimizationGuideTabUrlProviderTest() = default;
  ~OptimizationGuideTabUrlProviderTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    otr_browser_window_ = CreateBrowserWindow();
    otr_browser_ = CreateBrowser(
        profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
        browser()->type(),
        /*hosted_app=*/false, otr_browser_window_.get());
    tab_url_provider_ =
        std::make_unique<OptimizationGuideTabUrlProvider>(profile());
  }

  void TearDown() override {
    tab_url_provider_.reset();
    // Also destroy |otr_browser_| before the profile. browser()'s destruction
    // is handled in BrowserWithTestWindowTest::TearDown().
    otr_browser_->tab_strip_model()->CloseAllTabs();
    otr_browser_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  Browser* otr_browser() const { return otr_browser_.get(); }

  OptimizationGuideTabUrlProvider* tab_url_provider() const {
    return tab_url_provider_.get();
  }

 private:
  std::unique_ptr<BrowserWindow> otr_browser_window_;
  std::unique_ptr<Browser> otr_browser_;
  std::unique_ptr<OptimizationGuideTabUrlProvider> tab_url_provider_;
};

TEST_F(OptimizationGuideTabUrlProviderTest, GetUrlsNoOpenTabs) {
  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::Days(90));
  EXPECT_TRUE(urls.empty());
}

TEST_F(OptimizationGuideTabUrlProviderTest, GetUrlsFiltersOutIncognitoTabs) {
  AddTab(otr_browser(), GURL("https://otrshouldskip.com"));
  AddTab(browser(), GURL("https://example.com"));
  AddTab(browser(), GURL("https://example2.com"));

  std::vector<GURL> urls =
      tab_url_provider()->GetUrlsOfActiveTabs(base::Days(90));
  EXPECT_THAT(urls, ElementsAre(GURL("https://example2.com"),
                                GURL("https://example.com")));
}
