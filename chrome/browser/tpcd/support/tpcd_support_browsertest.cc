// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
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
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::URLLoaderInterceptor;
using content::WebContents;

namespace tpcd::trial {
namespace {

const char kTestTokenPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=,fMS4mpO6buLQ/QMd+zJmxzty/"
    "VQ6B1EUZqoCU04zoRU=";

const char kTrialEnabledDomain[] = "example.test";
const char kTrialEnabledSubdomain[] = "sub.example.test";
const char kTrialEnabledIframePath[] = "origin-trial-iframe";
const char kEmbeddedScriptPagePath[] =
    "tpcd/page_with_cross_site_tpcd_support_ot.html";
const char kSubdomainMatchingEmbeddedScriptPagePath[] =
    "tpcd/page_with_cross_site_tpcd_support_ot_with_subdomain_matching.html";
// Origin Trials token for `kTrialEnabledSite` generated with:
// tools/origin_trials/generate_token.py  https://example.test Tpcd
// --expire-days 5000
const char kTrialToken[] =
    "A1F5vUG256mdaDWxcpAddjWWg7LdOPuoEBswgFVy8b3j0ejT56eJ+e+"
    "IBocST6j2C8nYcnDm6gkd5O7M3FMo4AIAAABPeyJvcmlnaW4iOiAiaHR0cHM6Ly"
    "9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiVHBjZCIsICJleHBpcnkiO"
    "iAyMTI0MzA4MDY1fQ==";

// Origin Trials token for `kTrialEnabledSite` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://example.test Tpcd
// --is-subdomain --expire-days 5000
const char kTrialSubdomainMatchingToken[] =
    "AwvUTouERi5ZSbMQGkQhzRCxh3hWd4mu1/"
    "d8CPaQGC3LGmelPVjpqV8VPvKHXNB6ES337b3xvLRsQ6Z/"
    "5TAjdQAAAABkeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cm"
    "UiOiAiVHBjZCIsICJleHBpcnkiOiAyMTMwODYwOTA1LCAiaXNTdWJkb21haW4iOiB0cnVlfQ="
    "=";

// Origin Trials token for `kTrialEnabledSiteSubdomain` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://sub.example.test Tpcd
// --is-subdomain --expire-days 5000
const char kSubdomainTrialSubdomainMatchingToken[] =
    "A1XUCMiQfJGkSpeUIg7HmIpY9YtNoANQivDQYA8DLujoJhNovnyi0Fu2huOKeooMwHvfPecmA/"
    "8uJbrgH28T6A8AAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIs"
    "ICJmZWF0dXJlIjogIlRwY2QiLCAiZXhwaXJ5IjogMjEzMzk2NzQwOCwgImlzU3ViZG9tYWluIj"
    "ogdHJ1ZX0=";

class ContentSettingChangeObserver : public content_settings::Observer {
 public:
  explicit ContentSettingChangeObserver(
      content::BrowserContext* browser_context,
      const GURL request_url,
      const GURL partition_url,
      ContentSettingsType setting_type)
      : browser_context_(browser_context),
        request_url_(request_url),
        partition_url_(partition_url),
        setting_type_(setting_type) {
    HostContentSettingsMapFactory::GetForProfile(browser_context_)
        ->AddObserver(this);
  }

  ~ContentSettingChangeObserver() override {
    HostContentSettingsMapFactory::GetForProfile(browser_context_)
        ->RemoveObserver(this);
  }
  void Wait() { run_loop_.Run(); }

 private:
  // content_settings::Observer overrides:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override {
    if (content_type_set.Contains(setting_type_) &&
        primary_pattern.Matches(request_url_) &&
        secondary_pattern.Matches(partition_url_)) {
      run_loop_.Quit();
    }
  }

  raw_ptr<content::BrowserContext> browser_context_;
  base::RunLoop run_loop_;
  GURL request_url_;
  GURL partition_url_;
  ContentSettingsType setting_type_;
};

}  // namespace

class TpcdTrialBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->Start());
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

    GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);
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

  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    std::string path = params->url_request.url.path().substr(1);
    std::string host = params->url_request.url.host();
    std::string query = params->url_request.url.query();

    if (path.find("tpcd/") == 0) {
      content::URLLoaderInterceptor::WriteResponse(
          base::StrCat(
              {"chrome/test/data/", params->url_request.url.path_piece()}),
          params->client.get());
      return true;
    }

    if (host != kTrialEnabledDomain && host != kTrialEnabledSubdomain) {
      return false;
    }

    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n";

    if (path == kTrialEnabledIframePath) {
      if (host == kTrialEnabledDomain) {
        base::StrAppend(&headers, {"Origin-Trial: ", kTrialToken, "\n"});
      }

      if (host == kTrialEnabledSubdomain) {
        if (query == "etld_plus_1_token") {
          base::StrAppend(
              &headers, {"Origin-Trial: ", kTrialSubdomainMatchingToken, "\n"});
        } else {
          base::StrAppend(
              &headers,
              {"Origin-Trial: ", kSubdomainTrialSubdomainMatchingToken, "\n"});
        }
      }
    }

    content::URLLoaderInterceptor::WriteResponse(headers,
                                                 /*body=*/"",
                                                 params->client.get());
    return true;
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  const GURL kTrialEnabledSite{base::StrCat({"https://", kTrialEnabledDomain})};
  const GURL kTrialEnabledSiteSubdomain{
      base::StrCat({"https://", kTrialEnabledSubdomain})};
};

IN_PROC_BROWSER_TEST_F(TpcdTrialBrowserTest,
                       EnabledAfterCrossSiteIframeResponse) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Verify `kTrialEnabledSite` does not have cookie access as a third-party.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, GURL(), {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSite` iframe that returns the origin trial token in it's HTTP
  // response headers.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSite, embedding_site,
        ContentSettingsType::TPCD_TRIAL);

    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
    setting_observer.Wait();
  }

  // Check that `kTrialEnabledSite` now has access to cookies as a third-party
  // when embedded by `embedding_site`.
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);

  // TODO (crbug.com/1466156): Actually attempt to read cookies for
  // `kTrialEnabledSite` as a third-party.

  // Check cookie access for `kTrialEnabledSite` with a different path.
  GURL enabled_site_diff_path =
      GURL(kTrialEnabledSite.spec() + "iframe_blank.html");

  EXPECT_EQ(settings->GetCookieSetting(enabled_site_diff_path, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Verify that a subsequent load of a resource from `kTrialEnabledSite` on the
  // embedding site without the token (`enabled_site_diff_path`) removes the
  // `TPCD_TRIAL` content setting for it it.
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), enabled_site_diff_path,
        embedding_site, ContentSettingsType::TPCD_TRIAL);
    ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId,
                                             enabled_site_diff_path));
    setting_observer.Wait();
  }

  // Verify `kTrialEnabledSite` no longer has cookie access.
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(enabled_site_diff_path, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TpcdTrialBrowserTest,
                       EnabledAfterCrossSiteIframeResponseWithSubdomainToken) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Verify `kTrialEnabledSite` and `kTrialEnabledSiteSubdomain` do not have
  // cookie access as third-parties.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, GURL(), {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSiteSubdomain, GURL(), {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSiteSubdomain,
                                       embedding_site, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSiteSubdomain` iframe that returns the subdomain matching
  // origin trial token for it's eTLD+1 (`kTrialEnabledSite`) in it's HTTP
  // response headers.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSiteSubdomain,
        embedding_site, ContentSettingsType::TPCD_TRIAL);

    GURL iframe_url = GURL(kTrialEnabledSiteSubdomain.spec() +
                           kTrialEnabledIframePath + "?etld_plus_1_token");
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
    setting_observer.Wait();
  }

  // Check that both `kTrialEnabledSiteSubdomain` and it's eTLD+1
  // (`kTrialEnabledSite`) now has access to cookies as a third-party when
  // embedded by `embedding_site`.
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSiteSubdomain,
                                       embedding_site, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);

  // TODO (crbug.com/1466156): Actually attempt to read cookies for
  // `kTrialEnabledSiteSubdomain` as a third-party.

  // Check cookie access for `kTrialEnabledSiteSubdomain` with a different path.
  EXPECT_EQ(settings->GetCookieSetting(
                GURL(kTrialEnabledSiteSubdomain.spec() + "iframe_blank.html"),
                embedding_site, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  GURL enabled_site_no_token =
      GURL(kTrialEnabledSite.spec() + "iframe_blank.html");
  // Verify that a subsequent load of a resource from
  // `kTrialEnabledSiteSubdomain`'s eTLD+1 (`kTrialEnabledSite`) on the
  // embedding site without the token (`enabled_site_diff_path`) removes the
  // `TPCD_TRIAL` content setting for it.
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), enabled_site_no_token,
        embedding_site, ContentSettingsType::TPCD_TRIAL);
    ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId,
                                             enabled_site_no_token));
    setting_observer.Wait();
  }

  // Verify `kTrialEnabledSiteSubdomain` and it's eTLD+1 (`kTrialEnabledSite`)
  // no longer have cookie access.
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSiteSubdomain,
                                       embedding_site, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TpcdTrialBrowserTest, EnabledAfterMetaTagAppend) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site{
      base::StrCat({"https://a.test/", kEmbeddedScriptPagePath})};

  // Navigate to a page with an embedded script (sourced from
  // `kTrialEnabledSite`), that enables the trial for `kTrialEnabledSite` by
  // appending a meta tag containing `kTrialEnabledSite`'s third-party origin
  // trial token to the head of the page.
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSite, embedding_site,
        ContentSettingsType::TPCD_TRIAL);

    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
    setting_observer.Wait();
  }

  // Verify that `kTrialEnabledSite` now has access to cookies as a third-party
  // when embedded by `embedding_site`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                kTrialEnabledSite, embedding_site, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
}

IN_PROC_BROWSER_TEST_F(TpcdTrialBrowserTest,
                       EnabledAfterMetaTagAppendWithSubdomainToken) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site{base::StrCat(
      {"https://a.test/", kSubdomainMatchingEmbeddedScriptPagePath})};

  // Navigate to a page with an embedded script (sourced from
  // `kTrialEnabledSite`), that enables the trial for `kTrialEnabledSite` and
  // subdomains of it by appending a meta tag containing `kTrialEnabledSite`'s
  // subdomain-matching third-party origin trial token to the head of the page.
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSite, embedding_site,
        ContentSettingsType::TPCD_TRIAL);

    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
    setting_observer.Wait();
  }

  // Verify that `kTrialEnabledSite` and `kTrialEnabledSiteSubdomain` now have
  // access to cookies as a third-party when embedded by `embedding_site`.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, embedding_site, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSiteSubdomain,
                                       embedding_site, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                kTrialEnabledSite, embedding_site, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                kTrialEnabledSiteSubdomain, embedding_site, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
}

// This test verifies that TPCD_TRIAL content settings are scoped to the
// embedded origin, in the case where a non-subdomain-matching origin trial
// token is used to enable the trial.
IN_PROC_BROWSER_TEST_F(TpcdTrialBrowserTest, TrialEnabledForTokenOriginScope) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSite` iframe that returns the origin trial token in it's HTTP
  // response headers.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSite, embedding_site,
        ContentSettingsType::TPCD_TRIAL);

    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
    setting_observer.Wait();
  }

  // Verify the format of the `TPCD_TRIAL` content setting.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          GetActiveWebContents()->GetBrowserContext());

  content_settings::SettingInfo setting_info;
  ASSERT_EQ(settings_map->GetContentSetting(kTrialEnabledSite, embedding_site,
                                            ContentSettingsType::TPCD_TRIAL,
                                            &setting_info),
            CONTENT_SETTING_ALLOW);

  // `setting_info.primary_pattern` should only match
  // `kTrialEnabledSite`.
  EXPECT_EQ(setting_info.primary_pattern,
            ContentSettingsPattern::FromURLNoWildcard(kTrialEnabledSite));
  EXPECT_TRUE(setting_info.primary_pattern.Matches(kTrialEnabledSite));
  EXPECT_FALSE(
      setting_info.primary_pattern.Matches(kTrialEnabledSiteSubdomain));

  // `setting_info.secondary_pattern` should match origins that are
  // same-site with `embedding_site`.
  EXPECT_EQ(
      setting_info.secondary_pattern,
      ContentSettingsPattern::FromURLToSchemefulSitePattern(embedding_site));
}

// This test verifies that TPCD_TRIAL content settings are scoped to
// subdomains of the token origin, when created as a result of a subdomain
// matching origin trial token being used.
IN_PROC_BROWSER_TEST_F(
    TpcdTrialBrowserTest,
    SubdomainMatchingTokenEnablesTrialOnlyForSubdomainsOfTokenOrigin) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSiteSubdomain` iframe that returns
  // `kTrialEnabledSiteSubdomain`'s subdomain matching origin trial token in
  // it's HTTP response headers.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSiteSubdomain,
        embedding_site, ContentSettingsType::TPCD_TRIAL);

    GURL iframe_url =
        GURL(kTrialEnabledSiteSubdomain.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
    setting_observer.Wait();
  }

  // Verify the format of the `TPCD_TRIAL` content setting.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(
          GetActiveWebContents()->GetBrowserContext());

  content_settings::SettingInfo setting_info;
  ASSERT_EQ(settings_map->GetContentSetting(
                kTrialEnabledSiteSubdomain, embedding_site,
                ContentSettingsType::TPCD_TRIAL, &setting_info),
            CONTENT_SETTING_ALLOW);

  // `setting_info.primary_pattern` should only match
  // `kTrialEnabledSiteSubdomain` (https://sub.example.test) and subdomains of
  // it.
  EXPECT_EQ(setting_info.primary_pattern,
            ContentSettingsPattern::FromURL(kTrialEnabledSiteSubdomain));
  EXPECT_TRUE(setting_info.primary_pattern.Matches(
      GURL("https://foo.sub.example.test")));
  EXPECT_FALSE(
      setting_info.primary_pattern.Matches(GURL("https://www.example.test")));
  EXPECT_FALSE(setting_info.primary_pattern.Matches(kTrialEnabledSite));

  // `setting_info.secondary_pattern` should match origins that are
  // same-site with `embedding_site`.
  EXPECT_EQ(
      setting_info.secondary_pattern,
      ContentSettingsPattern::FromURLToSchemefulSitePattern(embedding_site));
}

}  // namespace tpcd::trial
