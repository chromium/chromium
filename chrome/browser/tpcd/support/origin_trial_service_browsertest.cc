// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/origin_trial_service.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/trial_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"

using content::URLLoaderInterceptor;
using content::WebContents;

namespace tpcd::trial {

namespace {
// Origin Trials token for `kTrialEnabledDomain` generated with:
// tools/origin_trials/generate_token.py  https://example.test
// LimitThirdPartyCookies --expire-days 1000
const char k1pOriginTrialToken[] =
    "A4rXpaazyMmgJ1ZFM0VTbJVkN6MUjDmBpTgPcW8Eo4NJ1HREVo0k7+C3+kG15uIMu/"
    "4g7bjRt9oD+"
    "EiiWa7Pqg8AAABheyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR"
    "1cmUiOiAiTGltaXRUaGlyZFBhcnR5Q29va2llcyIsICJleHBpcnkiOiAxODA0MTg0MjE4fQ==";

// Origin Trials token for `kTrialEnabledDomain` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://example.test
// LimitThirdPartyCookies --is-subdomain --expire-days 1000
const char k1pOriginTrialSubdomainMatchingToken[] =
    "A+"
    "NeirFywr04ZJyST5qNSP5gHYkbsvrJhd2DZrmgqxtqcobc89BaLAiLzV51Pmf73gaSozjBljgg"
    "eqecVcuE/"
    "Q4AAAB2eyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiA"
    "iTGltaXRUaGlyZFBhcnR5Q29va2llcyIsICJleHBpcnkiOiAxODA0MTg0MTY4LCAiaXNTdWJkb"
    "21haW4iOiB0cnVlfQ==";

// Origin Trials token for `kTrialEnabledSiteSubdomain` generated with:
// tools/origin_trials/generate_token.py  https://sub.example.test
// LimitThirdPartyCookies --expire-days 1000
const char kSubdomain1pOriginTrialToken[] =
    "A79E1oRwWqh+tVNJ1LalYnrfd9DZOhYmj6V85KXkiYeFbC97V1Bg6FPq+"
    "lxMNrIY5hDazYMBVkyGC/"
    "sdVstIeQAAAABleyJvcmlnaW4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIsICJm"
    "ZWF0dXJlIjogIkxpbWl0VGhpcmRQYXJ0eUNvb2tpZXMiLCAiZXhwaXJ5IjogMTgwNDE4OTQyN3"
    "0=";

// Origin Trials token for `kTrialEnabledSiteSubdomain` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://sub.example.test
// LimitThirdPartyCookies --is-subdomain --expire-days 1000
const char kSubdomain1pOriginTrialSubdomainMatchingToken[] =
    "AzosDlpG4yrKAm0NVvtYqeO7yRjli5018CiXPMGxsFIMrBpYFvAYlGQgg8/"
    "yhs5H5WvrUrvnjbXvHK+"
    "b28bccgUAAAB6eyJvcmlnaW4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIsICJmZ"
    "WF0dXJlIjogIkxpbWl0VGhpcmRQYXJ0eUNvb2tpZXMiLCAiZXhwaXJ5IjogMTgwNDE4OTQ2NCw"
    "gImlzU3ViZG9tYWluIjogdHJ1ZX0=";

// Origin Trials token for `kOtherTrialEnabledDomain` generated with:
// tools/origin_trials/generate_token.py  https://other.test
// LimitThirdPartyCookies --expire-days 1000
const char kOtherDomain1pOriginTrialToken[] =
    "A9/u+zN/DMWbxg0fafsYuMc7svS/X8qGRcSeehuA886viCDAJQea/"
    "GDAOLaTgA2C0UD98mOkmd1Qi2oiYXb2SAcAAABfeyJvcmlnaW4iOiAiaHR0cHM6Ly9vdGhlci5"
    "0ZXN0OjQ0MyIsICJmZWF0dXJlIjogIkxpbWl0VGhpcmRQYXJ0eUNvb2tpZXMiLCAiZXhwaXJ5I"
    "jogMTgwNDE4OTU0Mn0=";

}  // namespace

class TpcdOriginTrialBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTopLevelTpcdOriginTrial, {}}},
        {content_settings::features::kTrackingProtection3pcd});

    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    https_server_ = std::make_unique<net::EmbeddedTestServer>(
        net::EmbeddedTestServer::TYPE_HTTPS);
    https_server_->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_->AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data/")));
    ASSERT_TRUE(https_server_->Start());

    // We use a URLLoaderInterceptor, rather than the EmbeddedTestServer, since
    // the origin trial token in the response is associated with a fixed
    // origin, whereas EmbeddedTestServer serves content on a random port.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [this](URLLoaderInterceptor::RequestParams* params) {
              return OnRequest(params);
            }));

    // GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, false);
  }

  void TearDownOnMainThread() override {
    https_server_.reset();
    url_loader_interceptor_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  Profile* GetProfile() {
    return Profile::FromBrowserContext(
        GetActiveWebContents()->GetBrowserContext());
  }

  PrefService* GetPrefs() {
    return user_prefs::UserPrefs::Get(
        GetActiveWebContents()->GetBrowserContext());
  }

  // Most other cookie-related content settings compare their primary patterns'
  // against embedded/requesting sites and their secondary patterns'
  // against top-level sites. This convenience function helps avoid confusion
  // since `TOP_LEVEL_TPCD_ORIGIN_TRIAL` content settings only use a primary
  // pattern which is compared against top-level sites.
  ContentSettingChangeObserver CreateTpcdOriginTrialSettingsObserver(GURL url) {
    return ContentSettingChangeObserver(
        GetProfile(), url, GURL(),
        ContentSettingsType::TOP_LEVEL_TPCD_ORIGIN_TRIAL);
  }

  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    std::string host = params->url_request.url.host();
    std::string path = params->url_request.url.path().substr(1);
    std::string query = params->url_request.url.query();

    if (host != kTrialEnabledDomain && host != kTrialEnabledSubdomain &&
        host != kOtherTrialEnabledDomain) {
      return false;
    }

    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n";
    std::string body = "";

    std::string token = "";
    if (host == kTrialEnabledDomain) {
      if (query == "subdomain_matching_token") {
        token = k1pOriginTrialSubdomainMatchingToken;
      } else if (query != "no_token") {
        token = k1pOriginTrialToken;
      }
    }

    if (host == kTrialEnabledSubdomain) {
      if (query == "etld_plus_1_token") {
        token = k1pOriginTrialSubdomainMatchingToken;
      } else if (query == "subdomain_matching_token") {
        token = kSubdomain1pOriginTrialSubdomainMatchingToken;
      } else if (query != "no_token") {
        token = kSubdomain1pOriginTrialToken;
      }
    }

    if (host == kOtherTrialEnabledDomain) {
      if (query != "no_token") {
        token = kOtherDomain1pOriginTrialToken;
      }
    }

    if (!token.empty()) {
      if (path.find("meta_tag") == 0) {
        body =
            "<html>\n"
            "<head>\n"
            "<meta http-equiv='origin-trial' "
            "content='" +
            token +
            "'>\n"
            "</head>\n"
            "<body></body>\n"
            "</html>\n";
      } else {
        base::StrAppend(&headers, {"Origin-Trial: ", token, "\n"});
      }
    }

    content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                 params->client.get());
    return true;
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  const GURL kTrialEnabledSite{base::StrCat({"https://", kTrialEnabledDomain})};
  const GURL kTrialEnabledSiteSubdomain{
      base::StrCat({"https://", kTrialEnabledSubdomain})};
  const GURL kOtherTrialEnabledSite{
      base::StrCat({"https://", kOtherTrialEnabledDomain})};
};

IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest, EnabledAfterHttpResponse) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie is allowed under `kTrialEnabledSite`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Navigate to a `kTrialEnabledSite` page that returns its origin trial token
  // in the HTTP response headers.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kTrialEnabledSite));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is blocked under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Verify that a subsequent load of a `kTrialEnabledSite` page
  // without the token removes the `TOP_LEVEL_TPCD_ORIGIN_TRIAL` content setting
  // for it.
  {
    GURL enabled_site_no_token = GURL(kTrialEnabledSite.spec() + "?no_token");
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(enabled_site_no_token);
    ASSERT_TRUE(content::NavigateToURL(web_contents, enabled_site_no_token));
    setting_observer.Wait();
  }

  // Verify third-party cookie access is no longer blocked under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest,
                       EnabledAfterHttpResponseWithEtldSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie is allowed under `kTrialEnabledSite`
  // and `kTrialEnabledSiteSubdomain`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Navigate to a `kTrialEnabledSiteSubdomain` page that returns the subdomain
  // matching origin trial token for it's eTLD+1 (`kTrialEnabledSite`) in the
  // HTTP response headers.
  {
    GURL url = GURL(kTrialEnabledSiteSubdomain.spec() + "?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is blocked  under
  // `kTrialEnabledSite` and `kTrialEnabledSiteSubdomain`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Verify that a subsequent load of a page from `kTrialEnabledSiteSubdomain`'s
  // eTLD+1 (`kTrialEnabledSite`) without the token removes the
  // `TOP_LEVEL_TPCD_ORIGIN_TRIAL` content setting for them.
  {
    GURL enabled_site_no_token = GURL(kTrialEnabledSite.spec() + "?no_token");
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(enabled_site_no_token);
    ASSERT_TRUE(content::NavigateToURL(web_contents, enabled_site_no_token));
    setting_observer.Wait();
  }

  // Verify third-party cookie access is no longer blocked under
  // `kTrialEnabledSite` or `kTrialEnabledSiteSubdomain`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest, EnabledUsingMetaTag) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page where its origin trial token is in a
  // meta tag in the head of the document.
  {
    GURL url = GURL(kTrialEnabledSite.spec() + "meta_tag");

    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is now blocked under
  // `kTrialEnabledSite`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest,
                       EnabledUsingMetaTagWithEtldSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSiteSubdomain` page where the subdomain
  // matching origin trial token for it's eTLD+1 (`kTrialEnabledSite`) is in a
  // meta tag in the head of the document.
  {
    GURL url =
        GURL(kTrialEnabledSiteSubdomain.spec() + "meta_tag?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Verify that third-party cookie access is now blocked under
  // `kTrialEnabledSite` and `kTrialEnabledSiteSubdomain`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

// This test verifies (when enabled using a subdomain matching token) the trial
// is only disabled if a document from the token origin is loaded, even if one
// of its subdomains originally enabled the trial.
IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest,
                       OnlyTokenOriginCanDisableTrial) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSiteSubdomain` page that returns the subdomain
  // matching origin trial token for it's eTLD+1 (`kTrialEnabledSite`) in the
  // HTTP response headers.
  {
    GURL url = GURL(kTrialEnabledSiteSubdomain.spec() + "?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is blocked under `kTrialEnabledSite`
  // and `kTrialEnabledSiteSubdomain`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Verify that subsequent loads of a pages from
  // `kTrialEnabledSiteSubdomain` and other subdomains of the token origin
  // (`kTrialEnabledSite`) without the token do not affect the
  // `TOP_LEVEL_TPCD_ORIGIN_TRIAL` content setting for them.
  {
    GURL enabled_site_subdomain_no_token =
        GURL(kTrialEnabledSiteSubdomain.spec() + "?no_token");
    ASSERT_TRUE(
        content::NavigateToURL(web_contents, enabled_site_subdomain_no_token));

    GURL other_subdomain_no_token = https_server_->GetURL(
        base::StrCat({"random-subdomain.", kTrialEnabledDomain}),
        "/title1.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents, other_subdomain_no_token));
  }

  // Since we can't deterministically wait for the OriginTrialService to do
  // nothing in response to page loads that shouldn't affect trial state,
  // navigate to a page that enables the trial for a different origin and wait
  // for the associated `TOP_LEVEL_TPCD_ORIGIN_TRIAL` settings to be created,
  // then check that the subdomain matching content setting for
  // `kTrialEnabledSite` still remains.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(kOtherTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kOtherTrialEnabledSite));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is still blocked under
  // `kTrialEnabledSite` and `kTrialEnabledSiteSubdomain`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest,
                       NoSettingCreatedIfTrialEnabledCrossSite) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = https_server_->GetURL("a.test", "/iframe_blank.html");

  // Verify third-party cookie access is allowed under `kTrialEnabledSite`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSite` iframe that returns its LimitThirdPartyCookies origin
  // trial token in its HTTP response headers.
  {
    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
    const std::string kIframeId = "test";  // defined in iframe_blank.html
    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
  }

  // Check the LimitThirdPartyCookies origin trial itself is enabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  content::OriginTrialsControllerDelegate* trial_delegate =
      web_contents->GetBrowserContext()->GetOriginTrialsControllerDelegate();

  EXPECT_TRUE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kLimitThirdPartyCookies,
      base::Time::Now()));

  // Verify that third-party cookie access is still allowed under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

// Since the LimitThirdPartyCookies origin trial itself can be enabled/disabled
// in a cross-site context despite the trial only being intended to support
// top-level sites, this test verifies that changes to the status of the trial
// for an origin when in a cross-site context doesn't affect an existing content
// setting created for it in a top-level context.
IN_PROC_BROWSER_TEST_F(TpcdOriginTrialBrowserTest,
                       CrossSiteOriginTrialStateChangesIgnored) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  // Verify third-party cookie access is already allowed under
  // `kTrialEnabledSite`.
  ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Enable the trial by navigating to a `kTrialEnabledSite` page that returns
  // its origin trial token in the HTTP response headers.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTpcdOriginTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kTrialEnabledSite));
    setting_observer.Wait();
    EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                         kTrialEnabledSite, {}, nullptr),
              CONTENT_SETTING_BLOCK);
  }

  GURL embedding_site = https_server_->GetURL("a.test", "/iframe_blank.html");
  const std::string kIframeId = "test";  // defined in iframe_blank.html

  // Enable the origin trial for `kTrialEnabledSite` (under `embedding_site`)
  // using a cross-site iframe embedded on `embedding_site`.
  {
    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
  }

  // Check the LimitThirdPartyCookies origin trial itself is enabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  content::OriginTrialsControllerDelegate* trial_delegate =
      web_contents->GetBrowserContext()->GetOriginTrialsControllerDelegate();

  EXPECT_TRUE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kLimitThirdPartyCookies,
      base::Time::Now()));

  // Navigate the iframe to a `kTrialEnabledSite` page without the token to
  // disable the origin trial for it (under `embedding_site`).
  {
    GURL iframe_url = GURL(kTrialEnabledSite.spec() + "?no_token");
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
  }

  // Check the LimitThirdPartyCookies origin trial itself is now disabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  EXPECT_FALSE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kLimitThirdPartyCookies,
      base::Time::Now()));

  // Verify that third-party cookie access is still blocked under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

}  // namespace tpcd::trial
