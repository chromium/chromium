// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/top_level_trial_service.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/trial_test_utils.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
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
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/features.h"
#include "net/cookies/cookie_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

using content::URLLoaderInterceptor;
using content::WebContents;

namespace tpcd::trial {

class TopLevelTpcdTrialBrowserTest : public InProcessBrowserTest {
 public:
  const std::string kUkmEventName =
      "ThirdPartyCookies.TopLevelDeprecationTrial";

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTopLevelTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    InProcessBrowserTest::SetUp();
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

    GetPrefs()->SetBoolean(prefs::kTrackingProtection3pcdEnabled, true);

    ukm_recorder_.emplace();
  }

  void TearDownOnMainThread() override {
    https_server_.reset();
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
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
  // since |TOP_LEVEL_TPCD_TRIAL| content settings only use a primary pattern
  // which is compared against top-level sites.
  ContentSettingChangeObserver CreateTopLevelTrialSettingsObserver(GURL url) {
    return ContentSettingChangeObserver(
        GetProfile(), url, GURL(), ContentSettingsType::TOP_LEVEL_TPCD_TRIAL);
  }

  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    std::string host = params->url_request.url.host();
    std::string path = params->url_request.url.path().substr(1);
    std::string query = params->url_request.url.query();

    if (host != kTrialEnabledDomain && host != kTrialEnabledSubdomain &&
        host != kOtherTrialEnabledDomain) {
      return false;
    }

    bool should_use_meta_tag = path.starts_with("meta_tag");
    // Redirects are fixed from `kTrialEnabledDomain` to
    // `kOtherTrialEnabledDomain`.
    bool should_redirect =
        path.starts_with("redirect") && host == kTrialEnabledDomain;
    bool should_include_critical_header = query == "critical";

    // To simulate a followed redirect, we have to first notify the client of
    // the redirect, and then also commit the response for the redirect's
    // destination page.
    if (should_redirect) {
      NotifyClientOfRedirect(params);
    }

    // For redirects, we need to commit the response for the redirect's
    // destination page, so we should get the token for the destination page,
    // not the request page.
    std::string token_host = should_redirect ? kOtherTrialEnabledDomain : host;
    std::string token = ChooseToken(token_host, query);

    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n";
    if (!should_use_meta_tag && !token.empty()) {
      base::StrAppend(&headers, {"Origin-Trial: ", token, "\n"});
      if (should_include_critical_header) {
        base::StrAppend(&headers, {"Critical-Origin-Trial: TopLevelTpcd\n"});
      }
    }

    std::string body = (should_use_meta_tag && !token.empty())
                           ? "<html>\n"
                             "<head>\n"
                             "<meta http-equiv='origin-trial' "
                             "content='" +
                                 token +
                                 "'>\n"
                                 "</head>\n"
                                 "<body></body>\n"
                                 "</html>\n"
                           : "";

    content::URLLoaderInterceptor::WriteResponse(headers, body,
                                                 params->client.get());
    return true;
  }

  ukm::TestAutoSetUkmRecorder& ukm_recorder() { return ukm_recorder_.value(); }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::optional<ukm::TestAutoSetUkmRecorder> ukm_recorder_;
  const GURL kTrialEnabledSite{base::StrCat({"https://", kTrialEnabledDomain})};
  const GURL kTrialEnabledSiteSubdomain{
      base::StrCat({"https://", kTrialEnabledSubdomain})};
  const GURL kOtherTrialEnabledSite{
      base::StrCat({"https://", kOtherTrialEnabledDomain})};

 private:
  // Fixed redirect from `kTrialEnabledSite` to `kOtherTrialEnabledSite`.
  // Includes the token for `kTrialEnabledSite` in the response headers.
  void NotifyClientOfRedirect(
      content::URLLoaderInterceptor::RequestParams* params) {
    net::RedirectInfo redirect_info;
    redirect_info.new_url = kOtherTrialEnabledSite;
    redirect_info.new_method = "GET";
    net::HttpResponseInfo info;
    info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(base::StringPrintf(
            "HTTP/1.1 301 Moved Permanently\n"
            "Content-Type: text/html\n"
            "Origin-Trial: %s\n"
            "Location: %s\n",
            k1pDeprecationTrialToken, kOtherTrialEnabledSite.spec().c_str())));
    auto response = network::mojom::URLResponseHead::New();
    response->headers = info.headers;
    response->headers->GetMimeType(&response->mime_type);
    response->encoded_data_length = 0;
    params->client->OnReceiveRedirect(redirect_info, std::move(response));
  }

  std::string ChooseToken(std::string host, std::string query) {
    if (query == "no_token") {
      return "";
    }

    if (host == kTrialEnabledDomain) {
      if (query == "subdomain_matching_token") {
        return k1pDeprecationTrialSubdomainMatchingToken;
      } else {
        return k1pDeprecationTrialToken;
      }
    }

    if (host == kTrialEnabledSubdomain) {
      if (query == "etld_plus_1_token") {
        return k1pDeprecationTrialSubdomainMatchingToken;
      } else if (query == "subdomain_matching_token") {
        return kSubdomain1pDeprecationTrialSubdomainMatchingToken;
      } else {
        return kSubdomain1pDeprecationTrialToken;
      }
    }

    if (host == kOtherTrialEnabledDomain) {
      return kOtherDomain1pDeprecationTrialToken;
    }

    // The host isn't one of our trial-enabled domains, so return no token.
    return "";
  }
};

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest, EnabledAfterHttpResponse) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie access isn't permitted under |kTrialEnabledSite|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate to a |kTrialEnabledSite| page that returns its origin trial token
  // in the HTTP response headers.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kTrialEnabledSite));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is now permitted under
  // |kTrialEnabledSite|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Verify that a subsequent load of a |kTrialEnabledSite| page
  // without the token removes the |TOP_LEVEL_TPCD_TRIAL| content setting for
  // it.
  {
    GURL enabled_site_no_token = GURL(kTrialEnabledSite.spec() + "?no_token");
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(enabled_site_no_token);
    ASSERT_TRUE(content::NavigateToURL(web_contents, enabled_site_no_token));
    setting_observer.Wait();
  }

  // Verify third-party cookie access is no longer permitted under
  // |kTrialEnabledSite|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       EnabledAfterHttpResponseWithEtldSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie access isn't permitted under |kTrialEnabledSite|
  // or |kTrialEnabledSiteSubdomain|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate to a |kTrialEnabledSiteSubdomain| page that returns the subdomain
  // matching origin trial token for it's eTLD+1 (|kTrialEnabledSite|) in the
  // HTTP response headers.
  {
    GURL url = GURL(kTrialEnabledSiteSubdomain.spec() + "?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is now permitted under
  // |kTrialEnabledSite| and |kTrialEnabledSiteSubdomain|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Verify that a subsequent load of a page from |kTrialEnabledSiteSubdomain|'s
  // eTLD+1 (|kTrialEnabledSite|) without the token removes the
  // |TOP_LEVEL_TPCD_TRIAL| content setting for them.
  {
    GURL enabled_site_no_token = GURL(kTrialEnabledSite.spec() + "?no_token");
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(enabled_site_no_token);
    ASSERT_TRUE(content::NavigateToURL(web_contents, enabled_site_no_token));
    setting_observer.Wait();
  }

  // Verify third-party cookie access is no longer permitted under
  // |kTrialEnabledSite| or |kTrialEnabledSiteSubdomain|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest, EnabledUsingMetaTag) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a |kTrialEnabledSite| page where its origin trial token is in a
  // meta tag in the head of the document.
  {
    GURL url = GURL(kTrialEnabledSite.spec() + "meta_tag");

    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that third-party cookie access is now permitted under
  // |kTrialEnabledSite|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSite, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       EnabledUsingMetaTagWithEtldSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a |kTrialEnabledSiteSubdomain| page where the subdomain
  // matching origin trial token for it's eTLD+1 (|kTrialEnabledSite|) is in a
  // meta tag in the head of the document.
  {
    GURL url =
        GURL(kTrialEnabledSiteSubdomain.spec() + "meta_tag?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Verify that third-party cookie access is now permitted under
  // |kTrialEnabledSite| and |kTrialEnabledSiteSubdomain|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSiteSubdomain, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSiteSubdomain, {},
                nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSite, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
}

// This test verifies (when enabled using a subdomain matching token) the trial
// is only disabled if a document from the token origin is loaded, even if one
// of its subdomains originally enabled the trial.
IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       OnlyTokenOriginCanDisableTrial) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a |kTrialEnabledSiteSubdomain| page that returns the subdomain
  // matching origin trial token for it's eTLD+1 (|kTrialEnabledSite|) in the
  // HTTP response headers.
  {
    GURL url = GURL(kTrialEnabledSiteSubdomain.spec() + "?etld_plus_1_token");
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Check that Top-level 3pcd Trial grants now permit third-party cookie access
  // under |kTrialEnabledSite| and |kTrialEnabledSiteSubdomain|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSiteSubdomain, {},
                nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSite, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);

  // Verify that subsequent loads of a pages from |kTrialEnabledSiteSubdomain|
  // and other subdomains of the token origin (|kTrialEnabledSite|) without the
  // token do not affect the |TOP_LEVEL_TPCD_TRIAL| content setting for them.
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

  // Since we can't deterministically wait for the TopLevelTrialService to do
  // nothing in response to page loads that shouldn't affect trial state,
  // navigate to a page that enables the trial for a different origin and wait
  // for the associated |TOP_LEVEL_TPCD_TRIAL| settings to be created, then
  // check that the subdomain matching content setting for |kTrialEnabledSite|
  // still remains.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(kOtherTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kOtherTrialEnabledSite));
    setting_observer.Wait();

    // Check that a Top-level 3pcd Trial grant now permits third-party cookie
    // access under |kOtherTrialEnabledSite|.
    EXPECT_EQ(
        settings->GetThirdPartyCookieAllowMechanism(
            GURL(), net::SiteForCookies(), kOtherTrialEnabledSite, {}, nullptr),
        content_settings::CookieSettingsBase::ThirdPartyCookieAllowMechanism::
            kAllowByTopLevel3PCD);
  }

  // Check that Top-level 3pcd Trial grants still permit third-party cookie
  // access under |kTrialEnabledSite| and |kTrialEnabledSiteSubdomain|.
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSiteSubdomain, {},
                nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), net::SiteForCookies(), kTrialEnabledSite, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       NoSettingCreatedIfTrialEnabledCrossSite) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = https_server_->GetURL("a.test", "/iframe_blank.html");

  // Verify third-party cookie access isn't permitted under |kTrialEnabledSite|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate the top-level page to `embedding_site` and update it to have an
  // `kTrialEnabledSite` iframe that returns its TopLevelTpcd origin trial token
  // in its HTTP response headers.
  {
    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
    const std::string kIframeId = "test";  // defined in iframe_blank.html
    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
  }

  // We can't deterministically wait for the `TopLevelTrialService` to not
  // update content settings and then emit this UMA metric, which is why we need
  // the `RunLoop` here:
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Clients.TPCD.TopLevelTpcd.CrossSiteTrialChange"),
      BucketsAre(
          base::Bucket(OriginTrialStatusChange::kDisabled, 0),
          base::Bucket(OriginTrialStatusChange::kDisabled_MatchesSubdomains, 0),
          base::Bucket(OriginTrialStatusChange::kEnabled, 1),
          base::Bucket(OriginTrialStatusChange::kEnabled_MatchesSubdomains,
                       0)));

  // Check the TopLevelTpcd origin trial itself is enabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  content::OriginTrialsControllerDelegate* trial_delegate =
      web_contents->GetBrowserContext()->GetOriginTrialsControllerDelegate();

  EXPECT_TRUE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kTopLevelTpcd, base::Time::Now()));

  // Verify that third-party cookie access is still NOT permitted under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

// Since the TopLevelTpcd origin trial itself can be enabled/disabled in a
// cross-site context despite the trial only being intended to support top-level
// sites, this test verifies that changes to the status of the trial for an
// origin when in a cross-site context doesn't affect an existing content
// setting created for it in a top-level context.
IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       CrossSiteOriginTrialStateChangesIgnored) {
  base::HistogramTester histograms;
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  // Verify third-party cookie access isn't already permitted under
  // `kTrialEnabledSite`.
  ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Enable the trial by navigating to a `kTrialEnabledSite` page that returns
  // its origin trial token in the HTTP response headers.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kTrialEnabledSite));
    setting_observer.Wait();
    EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                         kTrialEnabledSite, {}, nullptr),
              CONTENT_SETTING_ALLOW);
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

  // We can't deterministically wait for the `TopLevelTrialService` to not
  // update content settings and then emit this UMA metric, which is why we need
  // the `RunLoop` here:
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Clients.TPCD.TopLevelTpcd.CrossSiteTrialChange"),
      BucketsAre(
          base::Bucket(OriginTrialStatusChange::kDisabled, 0),
          base::Bucket(OriginTrialStatusChange::kDisabled_MatchesSubdomains, 0),
          base::Bucket(OriginTrialStatusChange::kEnabled, 1),
          base::Bucket(OriginTrialStatusChange::kEnabled_MatchesSubdomains,
                       0)));

  // Check the TopLevelTpcd origin trial itself is enabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  content::OriginTrialsControllerDelegate* trial_delegate =
      web_contents->GetBrowserContext()->GetOriginTrialsControllerDelegate();

  EXPECT_TRUE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kTopLevelTpcd, base::Time::Now()));

  // Navigate the iframe to a `kTrialEnabledSite` page without the token to
  // disable the origin trial for it (under `embedding_site`).
  {
    GURL iframe_url = GURL(kTrialEnabledSite.spec() + "?no_token");
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
  }

  // We can't deterministically wait for the `TopLevelTrialService` to not
  // update content settings and then emit this UMA metric, which is why we need
  // the `RunLoop` here:
  base::RunLoop().RunUntilIdle();
  ASSERT_THAT(
      histograms.GetAllSamples(
          "PageLoad.Clients.TPCD.TopLevelTpcd.CrossSiteTrialChange"),
      BucketsAre(
          base::Bucket(OriginTrialStatusChange::kDisabled, 1),
          base::Bucket(OriginTrialStatusChange::kDisabled_MatchesSubdomains, 0),
          base::Bucket(OriginTrialStatusChange::kEnabled, 1),
          base::Bucket(OriginTrialStatusChange::kEnabled_MatchesSubdomains,
                       0)));

  // Check the TopLevelTpcd origin trial itself is now disabled for
  // `kTrialEnabledSite` embedded under `embedding_site`.
  EXPECT_FALSE(trial_delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(kTrialEnabledSite),
      url::Origin::Create(embedding_site),
      blink::mojom::OriginTrialFeature::kTopLevelTpcd, base::Time::Now()));

  // Verify that third-party cookie access is still permitted under
  // `kTrialEnabledSite`.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       UkmEmittedWhenTrialConfiguredViaResponseHeader) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page that returns its origin trial token
  // in the Origin-Trial HTTP response header.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, kTrialEnabledSite));
    setting_observer.Wait();
  }

  // Expect a UKM event to have been emitted for enabling the trial.
  auto ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, kTrialEnabledSite);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", true);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);

  // Load a subsequent `kTrialEnabledSite` page without the token to remove the
  // `TOP_LEVEL_TPCD_TRIAL` content setting for the site.
  GURL enabled_site_no_token = GURL(kTrialEnabledSite.spec() + "?no_token");
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(kTrialEnabledSite);
    ASSERT_TRUE(content::NavigateToURL(web_contents, enabled_site_no_token));
    setting_observer.Wait();
  }

  // Expect a UKM event to have been emitted for disabling the trial.
  ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 2u);
  entry = ukm_entries.at(1);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, enabled_site_no_token);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", false);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       UkmEmittedWhenTrialEnabledViaMetaTag) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page where its origin trial token is in a
  // meta tag in the head of the document.
  GURL url = GURL(kTrialEnabledSite.spec() + "meta_tag");
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Expect a UKM event to have been emitted for enabling the trial.
  auto ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", true);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);
}

IN_PROC_BROWSER_TEST_F(
    TopLevelTpcdTrialBrowserTest,
    UkmNotDuplicatedWhenResponseToNavigationRequestHasCriticalOriginTrialHeader) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page that returns its origin trial token
  // in the Origin-Trial HTTP response header and also returns a
  // `Critical-Origin-Trial: TopLevelTpcd` header.
  GURL url = GURL(kTrialEnabledSite.spec() + "?critical");
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(url);
    ASSERT_TRUE(content::NavigateToURL(web_contents, url));
    setting_observer.Wait();
  }

  // Expect only 1 UKM event to have been emitted for enabling the trial.
  auto ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, url);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", true);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       UkmNotEmittedWhenRedirectResponseHasTokenInHeader) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page that returns its origin trial token
  // in the Origin-Trial HTTP response header, while redirecting to another site
  // (`kOtherTrialEnabledSite`) that also returns its own origin trial token.
  GURL url = GURL(kTrialEnabledSite.spec() + "redirect");
  {
    ContentSettingChangeObserver settings_observer =
        CreateTopLevelTrialSettingsObserver(kOtherTrialEnabledSite);
    bool nav_success = content::NavigateToURL(
        web_contents, url, /*expected_commit_url=*/kOtherTrialEnabledSite);
    ASSERT_TRUE(nav_success);
    settings_observer.Wait();
  }

  // Expect a UKM event to have been emitted for enabling the trial on
  // `kOtherTrialEnabledSite` in the response headers. Expect no UKM event to
  // have been emitted for the redirect.
  auto ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, kOtherTrialEnabledSite);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", true);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);
}

IN_PROC_BROWSER_TEST_F(
    TopLevelTpcdTrialBrowserTest,
    UkmNotEmittedWhenRedirectResponseHasCriticalHeaderAndTokenInHeader) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Navigate to a `kTrialEnabledSite` page that returns its origin trial token
  // in the Origin-Trial HTTP response header, as well as the
  // `Critical-Origin-Trials: TopLevelTpcd` header, while redirecting to another
  // site (`kOtherTrialEnabledSite`) that also returns its own origin trial
  // token.
  GURL url = GURL(kTrialEnabledSite.spec() + "redirect?critical");
  {
    ContentSettingChangeObserver settings_observer =
        CreateTopLevelTrialSettingsObserver(kOtherTrialEnabledSite);
    bool nav_success = content::NavigateToURL(
        web_contents, url, /*expected_commit_url=*/kOtherTrialEnabledSite);
    ASSERT_TRUE(nav_success);
    settings_observer.Wait();
  }

  // Expect a UKM event to have been emitted for enabling the trial on
  // `kOtherTrialEnabledSite` in the response headers. Expect no UKM event to
  // have been emitted for the redirect.
  auto ukm_entries = ukm_recorder().GetEntriesByName(kUkmEventName);
  ASSERT_EQ(ukm_entries.size(), 1u);
  auto entry = ukm_entries.at(0);
  ukm_recorder().ExpectEntrySourceHasUrl(entry, kOtherTrialEnabledSite);
  ukm_recorder().ExpectEntryMetric(entry, "Enabled", true);
  ukm_recorder().ExpectEntryMetric(entry, "MatchSubdomains", false);
}

}  // namespace tpcd::trial
