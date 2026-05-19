// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/search.h"

#include <stddef.h>

#include <array>
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
#include "chrome/browser/supervised_user/supervised_user_test_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/supervised_user/supervision_mixin.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
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

class SearchTest : public MixinBasedInProcessBrowserTest {
 public:
  Profile* profile() const { return browser()->profile(); }

 protected:
  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();

    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host() == "foo.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 200 OK\nContent-Type: text/html\n",
                    "<html><body>OK</body></html>", params->client.get());
                return true;
              }
              if (params->url_request.url.host() == "insecure.com") {
                content::URLLoaderInterceptor::WriteResponse(
                    "HTTP/1.1 403 Forbidden\nContent-Type: text/html\n",
                    "<html><body>Forbidden</body></html>",
                    params->client.get());
                return true;
              }
              return false;
            }));

    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);
    SetSearchProvider(true, false);
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
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
        contents->GetPrimaryMainFrame()->GetProcess()->GetDeprecatedID());
  }

  // Each test case represents a navigation to |start_url| followed by a
  // navigation to |end_url|. We will check whether each navigation lands in an
  // Instant process, and also whether the navigation from start to end re-uses
  // the same SiteInstance, RenderViewHost, etc.
  // Note that we need to define this here because the flags needed to check
  // content::CanSameSiteMainFrameNavigationsChangeSiteInstances() etc might not
  // be set yet if we define this immediately (e.g. outside of the test class).
  struct ProcessIsolationTestCase {
    const char* description;
    const char* start_url;
    bool start_in_instant_process;
    const char* end_url;
    bool end_in_instant_process;
    bool same_site_instance;
    bool same_rvh;
    bool same_process;
  };
  const std::array<ProcessIsolationTestCase, 5> kProcessIsolationTestCases = {{
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
       !content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts(), true},
      {"Regular -> SRP", "https://foo.com/other", false, "https://foo.com/url",
       false, !content::CanSameSiteMainFrameNavigationsChangeSiteInstances(),
       !content::CanSameSiteMainFrameNavigationsChangeRenderFrameHosts(), true},
  }};

 private:
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
};

struct SearchTestCase {
  const char* url;
  bool expected_result;
  const char* comment;
};

IN_PROC_BROWSER_TEST_F(SearchTest, ShouldAssignURLToInstantRenderer) {
  // Only remote NTPs and most-visited tiles embedded in remote NTPs should be
  // assigned to Instant renderers.
  const auto kTestCases = std::to_array<SearchTestCase>({
      {"chrome-search://most-visited/title.html?bar=abc", true,
       "Most-visited tile"},
      {"https://foo.com/newtab", true, "Remote NTP"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/url", false, "Instant support was removed"},
      {"https://foo.com/alt", false, "Instant support was removed"},
      {"http://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/instant", false, "Instant support was removed"},
      {"https://foo.com/", false, "Instant support was removed"},
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result,
              ShouldAssignURLToInstantRenderer(GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

IN_PROC_BROWSER_TEST_F(SearchTest, ShouldUseProcessPerSiteForInstantSiteURL) {
  const auto kTestCases = std::to_array<SearchTestCase>({
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
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result, ShouldUseProcessPerSiteForInstantSiteURL(
                                        GURL(test.url), profile()))
        << test.url << " " << test.comment;
  }
}

IN_PROC_BROWSER_TEST_F(SearchTest, ProcessIsolation) {
  for (size_t i = 0; i < std::size(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://blank")));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(test.start_url)));
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(test.end_url)));

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

IN_PROC_BROWSER_TEST_F(SearchTest, ProcessIsolation_RendererInitiated) {
  for (size_t i = 0; i < std::size(kProcessIsolationTestCases); ++i) {
    const ProcessIsolationTestCase& test = kProcessIsolationTestCases[i];
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL("chrome://blank")));
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Navigate to start URL.
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(test.start_url)));
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
    content::TestNavigationObserver observer(contents);
    ASSERT_TRUE(content::ExecJs(
        contents, content::JsReplace("location.href = $1;", test.end_url)));
    observer.Wait();

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
    {"http://insecure.com/newtab", false, "Insecure New tab URL"},
};

IN_PROC_BROWSER_TEST_F(SearchTest, InstantNTPExtendedEnabled) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://blank")));
  for (const SearchTestCase& test : kInstantNTPTestCases) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(test.url)));

    EXPECT_EQ(
        test.expected_result,
        IsInstantNTP(browser()->tab_strip_model()->GetActiveWebContents()))
        << test.url << " " << test.comment;
  }
}

IN_PROC_BROWSER_TEST_F(SearchTest, InstantCacheableNTPNavigationEntry) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("chrome://blank")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller = contents->GetController();
  // Local NTP.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), chrome::ChromeUINewTabPageURLAsGURL()));
  EXPECT_FALSE(
      NavEntryIsInstantNTP(contents, controller.GetLastCommittedEntry()));
  // Remote NTP.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GetNewTabPageURL(profile())));
  EXPECT_TRUE(
      NavEntryIsInstantNTP(contents, controller.GetLastCommittedEntry()));
}

IN_PROC_BROWSER_TEST_F(SearchTest,
                       InstantCacheableNTPNavigationEntryNewProfile) {
  SetSearchProvider(false, false);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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

IN_PROC_BROWSER_TEST_F(SearchTest, NoRewriteInIncognito) {
  Profile* incognito =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(GURL(), GetNewTabPageURL(incognito));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_FALSE(HandleNewTabURLRewrite(&new_tab_url, incognito));
  EXPECT_EQ(chrome::ChromeUINewTabURLAsGURL(), new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, UseLocalNTPIfNTPURLIsInsecure) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(true, true);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, UseLocalNTPIfNTPURLIsNotSet) {
  // Set an insecure new tab page URL and verify that it's ignored.
  SetSearchProvider(false, true);
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

class SupervisedSearchTest : public SearchTest {
 protected:
  supervised_user::SupervisionMixin supervision_mixin_{
      mixin_host_,
      this,
      embedded_test_server(),
      {.sign_in_mode =
           supervised_user::SupervisionMixin::SignInMode::kSupervised}};
};

IN_PROC_BROWSER_TEST_F(SupervisedSearchTest,
                       UseLocalNTPIfNTPURLIsBlockedForSupervisedUser) {
  // Enable supervision, otherwise the URL filter won't be checked.
  // Block access to foo.com in the URL filter.
  supervised_user_test_util::SetManualFilterForHost(profile(), "foo.com",
                                                    /*allowlist=*/false);

  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL,
            GetNewTabPageURL(profile()));
  GURL new_tab_url = chrome::ChromeUINewTabURLAsGURL();
  EXPECT_TRUE(HandleNewTabURLRewrite(&new_tab_url, profile()));
  EXPECT_EQ(chrome::kChromeUINewTabPageThirdPartyURL, new_tab_url);
}

IN_PROC_BROWSER_TEST_F(SearchTest, IsNTPOrRelatedURL) {
  GURL invalid_url;
  const GURL& ntp_url = chrome::ChromeUINewTabURLAsGURL();

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
IN_PROC_BROWSER_TEST_F(SearchTest, IsNTPURL) {
  const auto kTestCases = std::to_array<SearchTestCase>({
      {"chrome-search://remote-ntp", true, "Remote NTP URL"},
      {"chrome://new-tab-page", true, "WebUI NTP"},
      {"chrome://new-tab-page/path?params", true,
       "WebUI NTP with path and params"},
      {"invalid-scheme://remote-ntp", false, "Invalid Remote NTP URL"},
      {"chrome-search://most-visited/", false, "Most visited URL"},
      {"", false, "Invalid URL"},
  });

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    const SearchTestCase& test = kTestCases[i];
    EXPECT_EQ(test.expected_result, IsNTPURL(GURL(test.url)))
        << test.url << " " << test.comment;
  }
}

// Regression test for https://crbug.com/40466271: Set up a search provider
// backed by localhost on a specific port, like browsertests do.  The
// chrome-search:// URLs generated in this mode should not have ports.
IN_PROC_BROWSER_TEST_F(SearchTest, SearchProviderWithPort) {
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
