// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/tab_matcher_desktop.h"

#include <array>
#include <memory>
#include <string>

#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#endif

class TabMatcherDesktopTest : public InProcessBrowserTest {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        ash::switches::kIgnoreUserProfileMappingForTests);
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
};

constexpr auto kServiceInitializers =
    std::to_array<TemplateURLService::Initializer>({
        {"kwa", "https://a.chromium.org/?a={searchTerms}", "ca"},
        {"kwb", "https://b.chromium.org/?b={searchTerms}", "cb"},
    });

IN_PROC_BROWSER_TEST_F(TabMatcherDesktopTest,
                       IsTabOpenWithURLNeverReturnsActiveTab) {
  std::unique_ptr<TemplateURLService> service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          GetProfile(), kServiceInitializers);
  TabMatcherDesktop matcher(service.get(), GetProfile());

  GURL foo("https://foo.chromium.org");
  GURL bar("https://bar.chromium.org");
  GURL baz("https://baz.chromium.org");

  for (auto url : {foo, bar, baz}) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  EXPECT_TRUE(matcher.IsTabOpenWithURL(foo, nullptr));
  EXPECT_TRUE(matcher.IsTabOpenWithURL(bar, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(baz, nullptr));
  EXPECT_FALSE(matcher.IsTabOpenWithURL(GURL("https://chromium.org"), nullptr));
}

IN_PROC_BROWSER_TEST_F(TabMatcherDesktopTest, GetOpenTabsOnlyWithinProfile) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://bar.chromium.org")));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://foo.chromium.org"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* second_profile = &profiles::testing::CreateProfileSync(
      profile_manager, profile_manager->GenerateNextProfileDirectoryPath());
  Browser* browser_with_second_profile = CreateBrowser(second_profile);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser_with_second_profile,
                                           GURL("https://baz.chromium.org")));

  std::unique_ptr<TemplateURLService> service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          GetProfile(), kServiceInitializers);
  TabMatcherDesktop matcher(service.get(), GetProfile());

  AutocompleteInput input;
  const auto tabs = matcher.GetOpenTabs(&input);
  ASSERT_EQ(tabs.size(), 2U);
  EXPECT_EQ(tabs[0].url, GURL("https://bar.chromium.org"));
  EXPECT_EQ(tabs[1].url, GURL("https://foo.chromium.org"));
}

IN_PROC_BROWSER_TEST_F(TabMatcherDesktopTest, IsTabOpenUsesCanonicalSearchURL) {
  std::unique_ptr<TemplateURLService> turl_service =
      TemplateURLServiceTestUtil::CreateTemplateURLServiceForTesting(
          GetProfile(), kServiceInitializers);
  TabMatcherDesktop matcher(turl_service.get(), GetProfile());

  TemplateURLData data;
  data.SetURL("https://example.com/search?q={searchTerms}");
  data.search_intent_params = {"intent"};
  TemplateURL turl(data);
  auto* default_turl = turl_service->Add(std::make_unique<TemplateURL>(data));
  turl_service->SetUserSelectedDefaultSearchProvider(default_turl);

  {
    TemplateURLRef::SearchTermsArgs search_terms_args(u"foo");
    search_terms_args.additional_query_params = "wiz=baz";
    std::string foo_url = default_turl->url_ref().ReplaceSearchTerms(
        search_terms_args, turl_service->search_terms_data());
    EXPECT_EQ("https://example.com/search?wiz=baz&q=foo", foo_url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(foo_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    // The last tab is active. IsTabOpenWithURL() does not match the active tab.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("https://active.chromium.org"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?q=foo"), nullptr));
    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?wiz=baz&q=foo"), nullptr));
    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?wiz=baz&intent=INTENT&q=foo"),
        nullptr));
  }
  {
    TemplateURLRef::SearchTermsArgs search_terms_args(u"bar");
    search_terms_args.additional_query_params = "intent=INTENT";
    std::string bar_url = default_turl->url_ref().ReplaceSearchTerms(
        search_terms_args, turl_service->search_terms_data());
    EXPECT_EQ("https://example.com/search?intent=INTENT&q=bar", bar_url);
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(bar_url), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    // The last tab is active. IsTabOpenWithURL() does not match the active tab.
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL("https://active.chromium.org"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?q=bar"), nullptr));
    EXPECT_FALSE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?wiz=baz&q=bar"), nullptr));
    EXPECT_TRUE(matcher.IsTabOpenWithURL(
        GURL("https://example.com/search?wiz=baz&intent=INTENT&q=bar"),
        nullptr));
  }
}
