// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

using TabMatcherDesktopTest = BrowserWithTestWindowTest;

const TemplateURLService::Initializer kServiceInitializers[] = {
    {"kwa", "a.chromium.org/?a={searchTerms}", "ca"},
    {"kwb", "b.chromium.org/?b={searchTerms}", "cb"},
};

TEST_F(TabMatcherDesktopTest, IsTabOpenWithURLNeverReturnsActiveTab) {
  std::unique_ptr<TemplateURLService> service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          profile(), kServiceInitializers);
  TabMatcherDesktop matcher(service.get(), profile());

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
  // The last tab added is active. It should be returned from `GetOpenTabs()`.
  AddTab(browser(), GURL("http://active.chromium.org"));
  AddTab(other_browser.get(), GURL("http://baz.chromium.org"));

  std::unique_ptr<TemplateURLService> service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          profile(), kServiceInitializers);
  TabMatcherDesktop matcher(service.get(), profile());

  AutocompleteInput input;
  const auto tabs = matcher.GetOpenTabs(&input);
  ASSERT_EQ(tabs.size(), 2U);
  EXPECT_EQ(tabs[0].url, GURL("http://bar.chromium.org"));
  EXPECT_EQ(tabs[1].url, GURL("http://foo.chromium.org"));

  other_browser->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabMatcherDesktopTest, IsTabOpenUsesCanonicalSearchURL) {
  std::unique_ptr<TemplateURLService> turl_service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          profile(), kServiceInitializers);
  TabMatcherDesktop matcher(turl_service.get(), profile());

  TemplateURLData data;
  data.SetURL("http://example.com/search?q={searchTerms}");
  data.search_intent_params = {"intent"};
  TemplateURL turl(data);
  auto* default_turl = turl_service->Add(std::make_unique<TemplateURL>(data));
  turl_service->SetUserSelectedDefaultSearchProvider(default_turl);

  {
    TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
    search_terms_args.additional_query_params = "wiz=baz";
    std::string foo_url = default_turl->url_ref().ReplaceSearchTerms(
        search_terms_args, turl_service->search_terms_data());
    EXPECT_EQ("http://example.com/search?wiz=baz&q=foo", foo_url);
    AddTab(browser(), GURL(foo_url));
    // The last tab is active. IsTabOpenWithURL() does not match the active tab.
    AddTab(browser(), GURL("http://active.chromium.org"));

    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?q=foo"), nullptr));
    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?wiz=baz&q=foo"), nullptr));
    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?wiz=baz&intent=INTENT&q=foo"),
        nullptr));
  }
  {
    TemplateURLRef::SearchTermsArgs search_terms_args(u"bar");
    search_terms_args.additional_query_params = "intent=INTENT";
    std::string bar_url = default_turl->url_ref().ReplaceSearchTerms(
        search_terms_args, turl_service->search_terms_data());
    EXPECT_EQ("http://example.com/search?intent=INTENT&q=bar", bar_url);
    AddTab(browser(), GURL(bar_url));
    // The last tab is active. IsTabOpenWithURL() does not match the active tab.
    AddTab(browser(), GURL("http://active.chromium.org"));

    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?q=bar"), nullptr));
    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?wiz=baz&q=bar"), nullptr));
    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("http://example.com/search?wiz=baz&intent=INTENT&q=bar"),
        nullptr));
  }
}

TEST_F(TabMatcherDesktopTest, IsTabOpenIncludeActiveTab) {
  std::unique_ptr<TemplateURLService> turl_service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          profile(), kServiceInitializers);
  TabMatcherDesktop matcher(turl_service.get(), profile());

  GURL foo("http://foo.chromium.org");
  GURL bar("http://bar.chromium.org");
  GURL baz("http://baz.chromium.org");

  for (auto url : {bar, baz}) {
    AddTab(browser(), url);
  }

  EXPECT_FALSE(
      matcher.IsTabOpenWithURL(foo, nullptr, /*exclude_active_tab =*/false));
  EXPECT_TRUE(
      matcher.IsTabOpenWithURL(bar, nullptr, /*exclude_active_tab =*/false));
  // The last tab is active. IsTabOpenWithURL() should match when
  // `exclude_active_tab` is false.
  EXPECT_TRUE(matcher.IsTabOpenWithURL(GURL("http://baz.chromium.org"), nullptr,
                                       /*exclude_active_tab =*/false));
  // The last tab is active. IsTabOpenWithURL() should not match when
  // `exclude_active_tab` is true.
  EXPECT_FALSE(matcher.IsTabOpenWithURL(GURL("http://baz.chromium.org"),
                                        nullptr,
                                        /*exclude_active_tab =*/true));
}

TEST_F(TabMatcherDesktopTest, IsTabOpenWithSameTitleOrSimilarURL) {
  std::unique_ptr<TemplateURLService> turl_service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          profile(), kServiceInitializers);
  TabMatcherDesktop matcher(turl_service.get(), profile());

  GURL foo("http://foo.org/path/#ref");
  AddTab(browser(), foo);
  auto* tab_1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationEntry* entry = tab_1->GetController().GetVisibleEntry();
  tab_1->UpdateTitleForEntry(entry, u"Test");
  GURL bar("http://bar.org");
  AddTab(browser(), bar);
  auto* tab_2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  content::NavigationEntry* entry_2 = tab_2->GetController().GetVisibleEntry();
  tab_2->UpdateTitleForEntry(entry_2, u"Testing");

  GURL::Replacements replacements;
  replacements.ClearQuery();

  // Tabs with same title and url should be considered matches.
  EXPECT_TRUE(matcher.IsTabOpenWithSameTitleOrSimilarURL(
      u"Testing", GURL("http://bar.org"), replacements,
      /*exclude_active_tab =*/false));

  // Tabs with same title should be considered matches even if urls are
  // different.
  EXPECT_TRUE(matcher.IsTabOpenWithSameTitleOrSimilarURL(
      u"Testing", GURL("http://different.org"), replacements,
      /*exclude_active_tab =*/false));

  // Sites with same path and different refs should be considered matches.
  EXPECT_TRUE(matcher.IsTabOpenWithSameTitleOrSimilarURL(
      u"Different name", GURL("http://foo.org/path/#differentref"),
      replacements,
      /*exclude_active_tab =*/false));

  // Sites with different path should not be considered matches.
  EXPECT_FALSE(matcher.IsTabOpenWithSameTitleOrSimilarURL(
      u"Different name", GURL("http://foo.org/differentpath/#ref"),
      replacements,
      /*exclude_active_tab =*/false));

  // Sites with same match and different refs and query params should be
  // considered matches.
  EXPECT_TRUE(matcher.IsTabOpenWithSameTitleOrSimilarURL(
      u"Different name", GURL("http://foo.org/path/#ref?param=123"),
      replacements,
      /*exclude_active_tab =*/false));
}
