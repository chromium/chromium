// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/browser/supervised_user_preferences.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

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
    data.SetShortName(u"foo.com");
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
        contents->GetPrimaryMainFrame()->GetProcess()->GetID());
  }

  // Each test case represents a navigation to |start_url| followed by a
  // navigation to |end_url|. We will check whether each navigation lands in an
  // Instant process, and also whether the navigation from start to end re-uses
  // the same SiteInstance, RenderViewHost, etc.
  // Note that we need to define this here because the flags needed to check
  // content::CanSameSiteMainFrameNavigationsChangeSiteInstances() etc might not
  // be set yet if we define this immediately (e.g. outside of the test class).
  const struct ProcessIsolationTestCase {
    const char* description;
    const char* start_url;
    bool start_in_instant_process;
    const char* end_url;
    bool end_in_instant_process;
    bool same_site_instance;
    bool same_rvh;
    bool same_process;
  } kProcessIsolationTestCases[5] = {
      {"Remote NTP -> SRP", "https://foo.com/newtab", true,
       "https://foo.com/url", false, false, false, false},
      {"Remote NTP -> Regular", "https://foo.com/newtab", true,
       "https://foo.com/other", false, false, false, false},
      {"SRP -> SRP", "https://foo.com/url", false, "https://foo.com/url", false,
       true,
       !content::WillSameSiteNavigationChangeRenderFrameHosts(
           /*is_main_frame=*/true),
       true},
      {"SRP -> Regular", "https://foo.com/url", false, "https://foo.com/other",
       false, !content::CanSameSiteMainFrameNavigationsChangeSiteInstances(),
       !content::CanSameSiteMainFrameNavigationsChangeSiteInstances(), true},
      {"Regular -> SRP", "https://foo.com/other", false, "https://foo.com/url",
       false, !content::CanSameSiteMainFrameNavigationsChangeSiteInstances(),
       !content::CanSameSiteMainFrameNavigationsChangeSiteInstances(), true},
  };

  // BrowserWithTestWindowTest:
  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                            test_url_loader_factory())}};
  }
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

TEST_F(SearchTest, ShouldAssignURLToInstantRenderer) {
  // Only remote NTPs and most-visited tiles embedded in remote NTPs should be
  // assigned to Instant renderers.
  const SearchTestCase kTestCases[] = {
      {"chrome-search://most-visited/title.html?bar=abc", true,
       "Most-visited tile"},
      {"https://foo.com/newtab", true, "Remote NTP"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/url", false, "Instant support was removed"},
      {"https://foo.com/alt", false, "Instant support was removed"},
      {"http://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/", false, "Instant support was removed"},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ShouldUseProcessPerSiteForInstantSiteURL) {
  const SearchTestCase kTestCases[] = {
      {"chrome-search://remote-ntp", true, "Remote NTP"},
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

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result, ShouldUseProcessPerSiteForInstantSiteURL(
                                        GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

TEST_F(SearchTest, ProcessIsolation) {
  for (size_t i = 0; i < std::size(kProcessIsolationTestCases); ++i) {
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
        contents->GetPrimaryMainFrame()->GetProcess();
    const content::RenderViewHost* start_rvh =
        contents->GetPrimaryMainFrame()->GetRenderViewHost();

    // Navigate to end URL.
    NavigateAndCommitActiveTab(GURL(test.end_url));
    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance.get() == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_rvh,
              start_rvh == contents->GetPrimaryMainFrame()->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_process,
              start_rph == contents->GetPrimaryMainFrame()->GetProcess())
        << test.description;
  }
}

TEST_F(SearchTest, ProcessIsolation_RendererInitiated) {
  for (size_t i = 0; i < std::size(kProcessIsolationTestCases); ++i) {
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
        contents->GetPrimaryMainFrame()->GetProcess();
    const content::RenderViewHost* start_rvh =
        contents->GetPrimaryMainFrame()->GetRenderViewHost();

    // Navigate to end URL via a renderer-initiated navigation.
    content::NavigationSimulator::NavigateAndCommitFromDocument(
        GURL(test.end_url), contents->GetPrimaryMainFrame());

    EXPECT_EQ(test.end_in_instant_process, InInstantProcess(contents))
        << test.description;

    EXPECT_EQ(test.same_site_instance,
              start_site_instance.get() == contents->GetSiteInstance())
        << test.description;
    EXPECT_EQ(test.same_rvh,
              start_rvh == contents->GetPrimaryMainFrame()->GetRenderViewHost())
        << test.description;
    EXPECT_EQ(test.same_process,
              start_rph == contents->GetPrimaryMainFrame()->GetProcess())
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

TEST_F(SearchTest, InstantCacheableNTPNavigationEntry) {
  AddTab(browser(), GURL("chrome://blank"));
  content::WebContents* contents =
        browser()->tab_strip_model()->GetWebContentsAt(0);
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  NavigateAndCommitActiveTab(GURL(chrome::kChromeUINewTabPageURL));
  EXPECT_FALSE(
      NavEntryIsInstantNTP(contents, controller.GetLastCommittedEntry()));
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
  // Test virtual url chrome://newtab for first NTP of a new profile
  EXPECT_TRUE(
      MatchesOriginAndPath(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                           controller.GetLastCommittedEntry()->GetURL()));
  // The new_tab_url gets set after the first NTP is visible.
  SetSearchProvider(true, false);
  EXPECT_TRUE(
      MatchesOriginAndPath(GURL(chrome::kChromeUINewTabPageThirdPartyURL),
                           controller.GetLastCommittedEntry()->GetURL()));
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
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

TEST_F(SearchTest, UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  // Mark the profile as supervised, otherwise the URL filter won't be checked.
  profile()->SetIsSupervisedProfile();
  // Block access to foo.com in the URL filter.
  supervised_user::SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile());
  supervised_user::SupervisedUserURLFilter* url_filter =
      supervised_user_service->GetURLFilter();
  std::map<std::string, bool> hosts;
  hosts["foo.com"] = false;
  url_filter->SetManualHosts(std::move(hosts));

  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url(chrome::kChromeUINewTabURL);
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

TEST_F(SearchTest, IsNTPOrRelatedURL) {
  GURL invalid_url;
  GURL ntp_url(chrome::kChromeUINewTabURL);

  EXPECT_FALSE(IsNTPOrRelatedURL(invalid_url, profile()));

  GURL remote_ntp_url(GetNewTabPageURL(profile()));
  GURL remote_ntp_service_worker_url("https://foo.com/newtab-serviceworker.js");
  GURL search_url_with_search_terms("https://foo.com/url?bar=abc");
  GURL search_url_without_search_terms("https://foo.com/url?bar");

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(remote_ntp_url, profile()));
  EXPECT_TRUE(IsNTPOrRelatedURL(remote_ntp_service_worker_url, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, profile()));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, profile()));

  EXPECT_FALSE(IsNTPOrRelatedURL(ntp_url, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(remote_ntp_url, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(remote_ntp_service_worker_url, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_with_search_terms, nullptr));
  EXPECT_FALSE(IsNTPOrRelatedURL(search_url_without_search_terms, nullptr));
}

// Tests whether a |url| corresponds to a New Tab page.
// See search::IsNTPURL(const GURL& url);
TEST_F(SearchTest, IsNTPURL) {
  const SearchTestCase kTestCases[] = {
      {"chrome-search://remote-ntp", true, "Remote NTP URL"},
      {"chrome://new-tab-page", true, "WebUI NTP"},
      {"chrome://new-tab-page/path?params", true,
       "WebUI NTP with path and params"},
      {"invalid-scheme://remote-ntp", false, "Invalid Remote NTP URL"},
      {"chrome-search://most-visited/", false, "Most visited URL"},
      {"", false, "Invalid URL"},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
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
  data.SetShortName(u"localhost");
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
