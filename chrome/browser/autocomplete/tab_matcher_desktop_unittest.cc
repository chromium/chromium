// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/template_url_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using TabMatcherDesktopTest = BrowserWithTestWindowTest;

const TemplateURLService::Initializer kServiceInitializers[] = {
    {"kwa", "a.chromium.org/?a={searchTerms}", "ca"},
    {"kwb", "b.chromium.org/?b={searchTerms}", "cb"},
};

TEST_F(TabMatcherDesktopTest, IsTabOpenWithURLNeverReturnsActiveTab) {
  TemplateURLService service(kServiceInitializers, 2);
  TabMatcherDesktop matcher(&service, profile());

  GURL foo("http://foo.chromium.org");
  GURL bar("http://bar.chromium.org");
  GURL baz("http://baz.chromium.org");

  for (auto url : {foo, bar, baz}) {
    AddTab(browser(), url);
  }

  EXPECT_TRUE(matcher.IsTabOpenWithURL(foo, nullptr));
  EXPECT_TRUE(matcher.IsTabOpenWithURL(bar, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(baz, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(GURL("http://chromium.org"), nullptr));
}

TEST_F(TabMatcherDesktopTest, GetOpenTabsOnlyWithinProfile) {
  TestingProfile* other_profile =
      profile_manager()->CreateTestingProfile("testing_other_profile");

  std::unique_ptr<BrowserWindow> other_window(CreateBrowserWindow());
  std::unique_ptr<Browser> other_browser(CreateBrowser(
      other_profile, browser()->type(), false, other_window.get()));

  AddTab(browser(), GURL("http://foo.chromium.org"));
  AddTab(browser(), GURL("http://bar.chromium.org"));
  AddTab(other_browser.get(), GURL("http://baz.chromium.org"));

  TemplateURLService service(kServiceInitializers, 2);
  TabMatcherDesktop matcher(&service, profile());

  EXPECT_EQ(matcher.GetOpenTabs().size(), 2U);

  other_browser->tab_strip_model()->CloseAllTabs();
}
