// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_url_filter.h"
#endif

namespace search {

bool MatchesOriginAndPath(const GURL& my_url, const GURL& other_url);

TEST(SearchURLsTest, MatchesOriginAndPath) {
  EXPECT_TRUE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                   GURL("http://example.com/path?param")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://not.example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_TRUE(MatchesOriginAndPath(GURL("http://example.com:80/path"),
                                   GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com:8080/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("ftp://example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                    GURL("https://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("https://example.com/path"),
                                    GURL("http://example.com/path")));
  EXPECT_FALSE(MatchesOriginAndPath(GURL("http://example.com/path"),
                                    GURL("http://example.com/another-path")));
}

class SearchTest : public BrowserWithTestWindowTest {
 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider(true, false);
  }

  virtual void SetSearchProvider(bool set_ntp_url, bool insecure_ntp_url) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("foo.com"));
    data.SetURL("http://foo.com/url?bar={searchTerms}");
    if (set_ntp_url) {
      data.new_tab_url = (insecure_ntp_url ? "http" : "https") +
                         std::string("://foo.com/newtab");
    }
    data.alternate_urls.push_back("http://foo.com/alt#quux={searchTerms}");

    TemplateURL* template_url =
        template_url_service->Add(std::make_unique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  bool InInstantProcess(content::WebContents* contents) {
    InstantService* instant_service =
        InstantServiceFactory::GetForProfile(profile());
    return instant_service->IsInstantProcess(
        contents->GetMainFrame()->GetProcess()->GetID());
  }
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRenderer) {
  // Only NTPs (both local and remote) should be assigned to Instant renderers.
  const SearchTestCase kTestCases[] = {
      {chrome::kChromeSearchLocalNtpUrl, true, "Local NTP"},
      {"chrome-search://local-ntp/local-ntp.html?bar=abc", true, "Local NTP"},
      {"https://foo.com/newtab", true, "Remote NTP"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/url", false, "Instant support was removed"},
      {"https://foo.com/alt", false, "Instant support was removed"},
      {"http://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/", false, "Instant support was removed"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ShouldUseProcessPerSiteForInstantURL) {
  const SearchTestCase kTestCases[] = {
      {"chrome-search://local-ntp", true, "Local NTP"},
      {"chrome-search://remote-ntp", true, "Remote NTP"},
      {"invalid-scheme://local-ntp", false, "Invalid Local NTP URL"},
      {"invalid-scheme://online-ntp", false, "Invalid Online NTP URL"},
      {"chrome-search://foo.com", false, "Search result page"},
      {"https://foo.com/instant", false, ""},
      {"https://foo.com/url", false, ""},
      {"https://foo.com/alt", false, ""},
      {"https://foo.com:80/instant", false, "HTTPS with port"},
      {"http://foo.com/instant", false, "Non-HTTPS"},
      {"http://foo.com:443/instant", false, "Non-HTTPS"},
      {"https://foo.com/instant", false, "No search terms replacement"},
      {"https://foo.com/", false, "Non-exact path"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldUseProcessPerSiteForInstantURL(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

// Each test case represents a navigation to |start_url| followed by a
// navigation to |end_url|. We will check whether each navigation lands in an
// Instant process, and also whether the navigation from start to end re-uses
// the same SiteInstance (and hence the same RenderViewHost, etc.).
const struct ProcessIsolationTestCase {
  const char* description;
  const char* start_url;
  bool start_in_instant_process;
  const char* end_url;
  bool end_in_instant_process;
  bool same_site_instance;
} kProcessIsolationTestCases[] = {
    {"Local NTP -> SRP", "chrome-search://local-ntp", true,
     "https://foo.com/url", false, false},
    {"Local NTP -> Regular", "chrome-search://local-ntp", true,
     "https://foo.com/other", false, false},
    {"Remote NTP -> SRP", "https://foo.com/newtab", true, "https://foo.com/url",
     false, false},
    {"Remote NTP -> Regular", "https://foo.com/newtab", true,
     "https://foo.com/other", false, false},
    {"SRP -> SRP", "https://foo.com/url", false, "https://foo.com/url", false,
     true},
    {"SRP -> Regular", "https://foo.com/url", false, "https://foo.com/other",
     false, true},
    {"Regular -> SRP", "https://foo.com/other", false, "https://foo.com/url",
     false, true},
};

TEST_F(SearchTest, ProcessIsolation) {
  for (size_t i = 0; i < base::size(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetMainFrame()->GetProcess();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL.
    NavigateAndCommitActiveTab(GURL(test.end_url));
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance.get() == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetMainFrame()->GetProcess())
        << test.description;
  }
}

TEST_F(SearchTest, ProcessIsolation_RendererInitiated) {
  for (size_t i = 0; i < base::size(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    AddTab(browser(), GURL("chrome://blank"));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    NavigateAndCommitActiveTab(GURL(test.start_url));
    EXPECT_EQ(test.start_in_instant_process, InInstantProcess(contents))
        << test.description;

    // Save state.
    const scoped_refptr<content::SiteInstance> start_site_instance =
        contents->GetSiteInstance();
    const content::RenderProcessHost* start_rph =
        contents->GetMainFrame()->GetProcess();
    const content::RenderViewHost* start_rvh =
        contents->GetRenderViewHost();

    // Navigate to end URL via a renderer-initiated navigation.
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(test.end_url), contents->GetMainFrame());

    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance.get() == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rvh == contents->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_site_instance,
              start_rph == contents->GetMainFrame()->GetProcess())
        << test.description;
  }
}

const SearchTestCase kInstantNTPTestCases[] = {
    {"https://foo.com/instant", false, "Instant support was removed"},
    {"https://foo.com/url", false, "Valid search URL"},
    {"https://foo.com/alt", false, "Valid alternative URL"},
    {"https://foo.com/url?bar=", false, "No query terms"},
    {"https://foo.com/url?bar=abc", false, "Has query terms"},
    {"http://foo.com/instant", false, "Insecure URL"},
    {"https://foo.com/instant", false, "No search term replacement"},
    {"chrome://blank/", false, "Chrome scheme"},
    {"chrome-search://foo", false, "Chrome-search scheme"},
    {"https://bar.com/instant", false, "Random non-search page"},
    {chrome::kChromeSearchLocalNtpUrl, true, "Local new tab page"},
    {"chrome-search://local-ntp/local-ntp.html?bar=abc", true,
     "Local new tab page"},
    {"https://foo.com/newtab", true, "New tab URL"},
    {"http://foo.com/newtab", false, "Insecure New tab URL"},
};

TEST_F(SearchTest, InstantNTPExtendedEnabled) {
  AddTab(browser(), GURL("chrome://blank"));
  for (const SearchTestCase& test : kInstantNTPTestCases) {
    NavigateAndCommitActiveTab(GURL(test.url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    EXPECT_EQ(test.expected_result, IsInstantNTP(contents))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantNTPCustomNavigationEntry) {
  AddTab(browser(), GURL("chrome://blank"));
  for (const SearchTestCase& test : kInstantNTPTestCases) {
    NavigateAndCommitActiveTab(GURL(test.url));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    content::NavigationController& controller = contents->GetController();
    controller.SetTransientEntry(
        content::NavigationController::CreateNavigationEntry(
            GURL("chrome://blank"), content::Referrer(), base::nullopt,
            ui::PAGE_TRANSITION_LINK, false, std::string(),
            contents->GetBrowserContext(),
            nullptr /* blob_url_loader_factory */));
    // The visible entry is now chrome://blank, but this is still an NTP.
    EXPECT_FALSE(NavEntryIsInstantNTP(contents, controller.GetVisibleEntry()));
    EXPECT_EQ(test.expected_result,
              NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()))
        << test.url << " " << test.comment;
    EXPECT_EQ(test.expected_result, IsInstantNTP(contents))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, InstantCacheableNTPNavigationEntry) {
  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  NavigateAndCommitActiveTab(GURL(chrome::kChromeSearchLocalNtpUrl));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // Remote NTP.
  NavigateAndCommitActiveTab(GetNewTabPageURL(profile()));
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, InstantCacheableNTPNavigationEntryNewProfile) {
  SetSearchProvider(false, false);
  AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Test virtual url chrome://newtab  for first NTP of a new profile
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
  // The new_tab_url gets set after the first NTP is visible.
  SetSearchProvider(true, false);
  EXPECT_TRUE(NavEntryIsInstantNTP(contents,
                                   controller.GetLastCommittedEntry()));
}

TEST_F(SearchTest, NoRewriteInIncognito) {
  TestingProfile* incognito =
      TestingProfile::Builder().BuildIncognito(profile());
  EXPECT_EQ(GURL(), GetNewTabPageURL(incognito));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_FALSE(HandleNewTabURLRewrite(&new_tab_url, incognito));
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL), new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
}

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
TEST_F(SearchTest, UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  // Mark the profile as supervised, otherwise the URL filter won't be checked.
  profile()->SetSupervisedUserId("supervised");
  // Block access to foo.com in the URL filter.
  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile());
  SupervisedUserURLFilter* url_filter = supervised_user_service->GetURLFilter();
  std::map<std::string, bool> hosts;
  hosts["foo.com"] = false;
  url_filter->SetManualHosts(std::move(hosts));

  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl),
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(GURL(chrome::kChromeSearchLocalNtpUrl), new_tab_url);
}
#endif

TEST_F(SearchTest, IsNTPOrRelatedURL) {
  GURL invalid_url;
  GURL ntp_url(chrome::kChromeUINewTabURL);
  GURL local_ntp_url(chrome::kChromeSearchLocalNtpUrl);
  GURL local_ntp_url_with_param(
      "chrome-search://local-ntp/local-ntp.html?bar=abc");

  EXPECT_FALSE(IsNTPOrRelatedURL(invalid_url, profile()));

  GURL remote_ntp_url(GetNewTabPageURL(profile()));
  GURL remote_ntp_service_worker_url("https://foo.com/newtab-serviceworker.js");
  GURL search_url_with_search_terms("https://foo.com/url?bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?bar");

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(local_ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(local_ntp_url_with_param, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(remote_ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(remote_ntp_service_worker_url, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, NULL));
  EXPECT_FALSE(IsNTPOrRelatedURL(local_ntp_url, NULL));
  EXPECT_FALSE(IsNTPOrRelatedURL(remote_ntp_url, NULL));
  EXPECT_FALSE(IsNTPOrRelatedURL(remote_ntp_service_worker_url, NULL));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, NULL));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, NULL));
}

// Tests whether a |url| corresponds to a New Tab page.
// See search::IsNTPURL(const GURL& url);
TEST_F(SearchTest, IsNTPURL) {
  const SearchTestCase kTestCases[] = {
      {"chrome-search://local-ntp", true, "Local NTP URL"},
      {"chrome-search://local-ntp/local-ntp.html?bar=abc", true,
       "Local NTP URL with params"},
      {"chrome-search://remote-ntp", true, "Remote NTP URL"},
      {"invalid-scheme://local-ntp", false, "Invalid Local NTP URL"},
      {"invalid-scheme://remote-ntp", false, "Invalid Remote NTP URL"},
      {"chrome-search://most-visited/", false, "Most visited URL"},
      {"", false, "Invalid URL"},
  };

  for (size_t i = 0; i < base::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result, IsNTPURL(GURL(test.url)))
        << test.url << " " << test.comment;
  }
}

// Regression test for https://crbug.com/605720: Set up a search provider backed
// by localhost on a specific port, like browsertests do.  The chrome-search://
// URLs generated in this mode should not have ports.
TEST_F(SearchTest, SearchProviderWithPort) {
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile());
  TemplateURLData data;
  data.SetShortName(base::ASCIIToUTF16("localhost"));
  data.SetURL("https://[::1]:1993/url?bar={searchTerms}");
  data.new_tab_url = "https://[::1]:1993/newtab";
  data.alternate_urls.push_back("https://[::1]:1993/alt#quux={searchTerms}");

  TemplateURL* template_url =
      template_url_service->Add(std::make_unique<TemplateURL>(data));
  template_url_service->SetUserSelectedDefaultSearchProvider(template_url);

  EXPECT_TRUE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1993/newtab?lala"), profile()));
  EXPECT_FALSE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1992/newtab?lala"), profile()));
  EXPECT_EQ(GURL("chrome-search://remote-ntp/newtab?lala"),
            GetEffectiveURLForInstant(GURL("https://[::1]:1993/newtab?lala"),
                                      profile()));
  EXPECT_FALSE(ShouldAssignURLToInstantRenderer(
      GURL("https://[::1]:1993/unregistered-path"), profile()));
}

}  // namespace search
