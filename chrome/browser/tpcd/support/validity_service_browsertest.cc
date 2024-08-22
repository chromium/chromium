// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/support/validity_service.h"

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/top_level_trial_service.h"
#include "chrome/browser/tpcd/support/top_level_trial_service_factory.h"
#include "chrome/browser/tpcd/support/tpcd_support_service.h"
#include "chrome/browser/tpcd/support/tpcd_support_service_factory.h"
#include "chrome/browser/tpcd/support/trial_test_utils.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
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

class ValidityServiceBrowserTestBase : public PlatformBrowserTest {
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

    // We use a URLLoaderInterceptor in tests that actually use an origin trial
    // token, rather than the EmbeddedTestServer, since the origin trial token
    // in the response is associated with a fixed origin, whereas
    // EmbeddedTestServer serves content on a random port.
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

  TopLevelTrialService* GetTopLevelTrialService() {
    return TopLevelTrialServiceFactory::GetForProfile(GetProfile());
  }

  TpcdTrialService* GetTpcdTrialService() {
    return TpcdTrialServiceFactory::GetForProfile(GetProfile());
  }

  ValidityService* GetValidityService() {
    return ValidityService::FromWebContents(GetActiveWebContents());
  }

  content::RenderFrameHost* GetIFrame() {
    content::WebContents* web_contents = GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  [[nodiscard]] bool NavigateIFrameAndWaitForCookieAccess(GURL iframe_url) {
    content::WebContents* web_contents = GetActiveWebContents();

    URLCookieAccessObserver observer(web_contents, iframe_url,
                                     CookieOperation::kChange);
    const std::string kIframeId = "test";  // defined in iframe_blank.html
    bool success =
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url);
    if (success) {
      observer.Wait();
    }
    return success;
  }

  void NavigateToPageWithIFrame(const std::string& iframe_host,
                                const std::string& embedding_host) {
    content::WebContents* web_contents = GetActiveWebContents();

    // Navigate the top-level page to |embedding_site|.
    GURL embedding_site =
        https_server_->GetURL(embedding_host, "/iframe_blank.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

    // Open an |iframe_host| iframe.
    GURL iframe_url = https_server_->GetURL(iframe_host, "/title1.html");
    const std::string kIframeId = "test";  // defined in iframe_blank.html
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
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

  // Creates a |TPCD_TRIAL| content setting allowing
  // |embedded_url| to access third-party cookies under |top_level_url|
  // without actually providing an origin trial token and enabling the
  // associated origin trial.
  void CreateAndVerifyThirdPartyTrialGrant(const GURL& embedded_url,
                                           const GURL& top_level_url,
                                           bool match_subdomains) {
    // Create the content setting.
    ContentSettingChangeObserver setting_observer(
        GetActiveWebContents()->GetBrowserContext(), embedded_url,
        top_level_url, ContentSettingsType::TPCD_TRIAL);
    GetTpcdTrialService()->Update3pcdTrialSettingsForTesting(
        OriginTrialStatusChangeDetails(url::Origin::Create(embedded_url),
                                       top_level_url.spec(), match_subdomains,
                                       /*enabled=*/true,
                                       /*source_id=*/std::nullopt));
    setting_observer.Wait();

    // Verify that a |TPCD_TRIAL| content setting now allows |embedded_url|
    // access to cookies as a third-party when embedded by |top_level_url|.
    content_settings::CookieSettings* settings =
        CookieSettingsFactory::GetForProfile(GetProfile()).get();
    ASSERT_EQ(settings->GetCookieSetting(embedded_url, net::SiteForCookies(),
                                         top_level_url, {}, nullptr),
              CONTENT_SETTING_ALLOW);
    ASSERT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                  embedded_url, net::SiteForCookies::FromUrl(top_level_url),
                  top_level_url, {}, nullptr),
              content_settings::CookieSettingsBase::
                  ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
  }

  // Creates a |TOP_LEVEL_TPCD_TRIAL| content setting allowing sites embedded
  // under |top_level_url| to access third-party cookies without actually
  // providing an origin trial token and enabling the associated origin trial.
  void CreateAndVerifyFirstPartyTrialGrant(const GURL& top_level_url,
                                           bool match_subdomains) {
    // Create the content setting.
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(top_level_url);
    GetTopLevelTrialService()->UpdateTopLevelTrialSettingsForTesting(
        url::Origin::Create(top_level_url), match_subdomains, /*enabled=*/true);
    setting_observer.Wait();

    // Verify that a |TOP_LEVEL_TPCD_TRIAL| content setting now allows all sites
    // access to cookies as a third-party when embedded by |top_level_url|.
    content_settings::CookieSettings* settings =
        CookieSettingsFactory::GetForProfile(GetProfile()).get();
    ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                         top_level_url, {}, nullptr),
              CONTENT_SETTING_ALLOW);
    ASSERT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                  GURL(), net::SiteForCookies(), top_level_url, {}, nullptr),
              content_settings::CookieSettingsBase::
                  ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  }

  virtual bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    return false;
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  const GURL kTrialEnabledSite{base::StrCat({"https://", kTrialEnabledDomain})};
  const GURL kTrialEnabledSiteSubdomain{
      base::StrCat({"https://", kTrialEnabledSubdomain})};
};

class ValidityService3pTrialBrowserTest
    : public ValidityServiceBrowserTestBase {
  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    PlatformBrowserTest::SetUp();
  }

  bool OnRequest(
      content::URLLoaderInterceptor::RequestParams* params) override {
    std::string host = params->url_request.url.host();
    std::string path = params->url_request.url.path().substr(1);

    if (host != kTrialEnabledDomain || path != kTrialEnabledIframePath) {
      return false;
    }

    URLLoaderInterceptor::WriteResponse(
        base::StrCat({"HTTP/1.1 200 OK\n", "Content-type: text/html\n",
                      "Origin-Trial: ", kTrialToken, "\n", "\n"}),
        "", params->client.get());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ValidityService3pTrialBrowserTest,
                       RemovesInvalidSettingOnJsCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame(kTrialEnabledSite.host(), "a.test");

  // Create a |TPCD_TRIAL| setting for |iframe_url| under |top_level_url|
  // without actually enabling the "Tpcd" trial.
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, top_level_url,
                                      /*match_subdomains=*/false);

  // Access cookies via javascript in the iframe, which should cause the setting
  // to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify |iframe_url| no longer has third-party cookie access when
  // embedded by |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService3pTrialBrowserTest,
                       RemoveInvalidSubdomainMatchingSettingOnJsCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame(kTrialEnabledSiteSubdomain.host(), "a.test");

  // Create a subdomain-matching |TPCD_TRIAL| setting for |grant_url|
  // (which |iframe_url| is a subdomain of) under |top_level_url| without
  // actually enabling the "Tpcd" trial.
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  GURL grant_url = GURL(base::StrCat(
      {"https://", kTrialEnabledSite.host(), ":", iframe_url.port()}));
  CreateAndVerifyThirdPartyTrialGrant(grant_url, top_level_url,
                                      /*match_subdomains=*/true);

  // Access cookies via javascript in the iframe, which should cause the setting
  // to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify |iframe_url| and |grant_url| no longer have
  // third-party cookie access when embedded by |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(grant_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService3pTrialBrowserTest,
                       RemoveInvalidSettingOnNavigationCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  // Navigate the top-level page to |top_level_url|.
  GURL top_level_url = https_server_->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, top_level_url));

  // Create a |TPCD_TRIAL| setting for |iframe_url| under |top_level_url|
  // without actually enabling the "Tpcd" trial.
  GURL iframe_url = https_server_->GetURL(
      kTrialEnabledSite.host(), "/set-cookie?name=value;Secure;SameSite=None");
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, top_level_url,
                                      /*match_subdomains=*/false);

  // Navigate the iframe to |iframe_url| to set a cookie via a network response,
  // which should cause the setting to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  ASSERT_TRUE(NavigateIFrameAndWaitForCookieAccess(iframe_url));
  setting_observer.Wait();

  // Verify |iframe_url| no longer has third-party cookie access when embedded
  // by |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(
    ValidityService3pTrialBrowserTest,
    RemoveInvalidSubdomainMatchingSettingOnNavigationCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  // Navigate the top-level page to |top_level_url|.
  GURL top_level_url = https_server_->GetURL("a.test", "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, top_level_url));

  // Create a subdomain-matching |TPCD_TRIAL| setting for |grant_url|
  // (which |iframe_url| is a subdomain of) under |top_level_url| without
  // actually enabling the "Tpcd" trial.
  GURL iframe_url =
      https_server_->GetURL(kTrialEnabledSiteSubdomain.host(),
                            "/set-cookie?name=value;Secure;SameSite=None");
  GURL grant_url = GURL(base::StrCat(
      {"https://", kTrialEnabledSite.host(), ":", iframe_url.port()}));
  CreateAndVerifyThirdPartyTrialGrant(grant_url, top_level_url,
                                      /*match_subdomains=*/true);

  // Navigate the iframe to |iframe_url| to set a cookie via a network response,
  // which should cause the setting to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  ASSERT_TRUE(NavigateIFrameAndWaitForCookieAccess(iframe_url));
  setting_observer.Wait();

  // Verify |iframe_url| and |grant_url| no longer have
  // third-party cookie access when embedded by |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(grant_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService3pTrialBrowserTest,
                       RemoveAllSettingsCreatedUsingAffectedToken) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame(kTrialEnabledSite.host(), "a.test");
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();

  // Create |TPCD_TRIAL| settings for |iframe_url| under various top-level
  // sites.
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, top_level_url,
                                      /*match_subdomains=*/false);
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, GURL("https://b.test"),
                                      /*match_subdomains=*/false);
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, GURL("https://c.test"),
                                      /*match_subdomains=*/false);
  // Note: this setting matches subdomains, while the others don't, meaning it
  // would've been created using a different origin trial token.
  GURL other_top_level_url("https://other-top-level.test");
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, other_top_level_url,
                                      /*match_subdomains=*/true);

  // Also, create a |TPCD_TRIAL| setting for |other_embedded_url| under
  // |top_level_url|.
  GURL other_embedded_url("https://another-embedded-site.test");
  CreateAndVerifyThirdPartyTrialGrant(other_embedded_url, top_level_url,
                                      /*match_subdomains=*/false);

  // Access cookies via javascript in the iframe, which should cause the setting
  // to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify |TPCD_TRIAL| content settings with the same primary pattern as the
  // setting that allowed 3PC access in the iframe have been removed.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       GURL("https://b.test"), {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       GURL("https://c.test"), {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Note: since the setting allowing |iframe_url| to access 3PC under
  // |other_top_level_url| matches subdomains and the setting that
  // allowed access in the iframe context did not, the setting for 3PC access
  // under |other_top_level_url| must've been created with another
  // token and should NOT be removed.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       other_top_level_url, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                iframe_url, net::SiteForCookies::FromUrl(other_top_level_url),
                other_top_level_url, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);

  // Verify |other_embedded_url| still has a |TPCD_TRIAL| grant for third-party
  // cookie access when embedded by |top_level_url|.
  EXPECT_EQ(
      settings->GetCookieSetting(other_embedded_url, net::SiteForCookies(),
                                 top_level_url, {}, nullptr),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                other_embedded_url, net::SiteForCookies::FromUrl(top_level_url),
                top_level_url, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
}

IN_PROC_BROWSER_TEST_F(
    ValidityService3pTrialBrowserTest,
    RemoveAllSettingsCreatedUsingAffectedSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame(kTrialEnabledSite.host(), "a.test");
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();

  // Create |TPCD_TRIAL| settings for |iframe_url| (and subdomains of it) under
  // various top-level sites.
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, top_level_url,
                                      /*match_subdomains=*/true);
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, GURL("https://b.test"),
                                      /*match_subdomains=*/true);
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, GURL("https://c.test"),
                                      /*match_subdomains=*/true);
  // Note: this setting does NOT match subdomains, while the others do, meaning
  // it would've been created using a different origin trial token.
  GURL other_top_level_url("https://other-top-level.test");
  CreateAndVerifyThirdPartyTrialGrant(iframe_url, other_top_level_url,
                                      /*match_subdomains=*/false);

  // Also, create a |TPCD_TRIAL| setting for |other_embedded_url| under
  // |top_level_url|.
  GURL other_embedded_url("https://another-embedded-site.test");
  CreateAndVerifyThirdPartyTrialGrant(other_embedded_url, top_level_url,
                                      /*match_subdomains=*/true);

  // Access cookies via javascript in the iframe, which should cause the setting
  // to be removed.
  ContentSettingChangeObserver setting_observer(
      web_contents->GetBrowserContext(), iframe_url, top_level_url,
      ContentSettingsType::TPCD_TRIAL);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify |TPCD_TRIAL| content settings with the same primary pattern as the
  // setting that allowed 3PC access in the iframe have been removed.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       GURL("https://b.test"), {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       GURL("https://c.test"), {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Note: since the setting allowing |iframe_url| to access 3PC under
  // |other_top_level_url| does NOT match subdomains and the setting
  // that allowed access in the iframe context did, the setting for 3PC access
  // under |other_top_level_url| must've been created with another
  // token and should NOT be removed.
  EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                       other_top_level_url, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                iframe_url, net::SiteForCookies::FromUrl(other_top_level_url),
                other_top_level_url, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);

  // Verify |other_embedded_url| still has a |TPCD_TRIAL| grant for
  // third-party cookie access when embedded by |top_level_url|.
  EXPECT_EQ(
      settings->GetCookieSetting(other_embedded_url, net::SiteForCookies(),
                                 top_level_url, {}, nullptr),
      CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                other_embedded_url, net::SiteForCookies::FromUrl(top_level_url),
                top_level_url, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowBy3PCD);
}

IN_PROC_BROWSER_TEST_F(ValidityService3pTrialBrowserTest,
                       PreserveValidSettings) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  GURL top_level_url =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");
  const std::string kIframeId = "test";  // defined in iframe_blank.html

  // Navigate the top-level page to |top_level_url| and update it to have an
  // |kTrialEnabledSite| iframe that returns the origin trial token in its HTTP
  // response headers.
  {
    ASSERT_TRUE(content::NavigateToURL(web_contents, top_level_url));

    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kTrialEnabledSite, top_level_url,
        ContentSettingsType::TPCD_TRIAL);

    GURL iframe_url = GURL(kTrialEnabledSite.spec() + kTrialEnabledIframePath);
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));
    setting_observer.Wait();
  }

  // Write a third-party cookie from the |kTrialEnabledSite| iframe.
  AccessCookieViaJsIn(web_contents, GetIFrame());

  // Since we can't deterministically wait for the ValidityService to do nothing
  // in response to a third-party cookie access permitted by a valid
  // |TPCD_TRIAL| content setting, also trigger a cookie access for a different
  // origin with an invalid setting, then after the invalid setting has been
  // removed, check that the |kTrialEnabledSite| content setting still remains.
  {
    GURL iframe_url =
        https_server_->GetURL("different-host.test", "/title1.html");
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));

    // Create a |TPCD_TRIAL| setting for |iframe_url| under |top_level_url|
    // without actually enabling the "Tpcd" trial.
    CreateAndVerifyThirdPartyTrialGrant(iframe_url, top_level_url,
                                        /*match_subdomains=*/false);

    // Access cookies via javascript in the iframe, which should cause the
    // setting to be removed.
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), iframe_url, top_level_url,
        ContentSettingsType::TPCD_TRIAL);
    AccessCookieViaJsIn(web_contents, GetIFrame());
    setting_observer.Wait();

    // Verify |iframe_url| no longer has access to third-party cookies when
    // embedded by |top_level_url|.
    EXPECT_EQ(settings->GetCookieSetting(iframe_url, net::SiteForCookies(),
                                         top_level_url, {}, nullptr),
              CONTENT_SETTING_BLOCK);
  }

  // Verify |kTrialEnabledSite| still has access to third-party cookies when
  // embedded by |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(kTrialEnabledSite, net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

class ValidityService1pTrialBrowserTest
    : public ValidityServiceBrowserTestBase {
  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTopLevelTpcdTrialSettings, {}},
         {content_settings::features::kTrackingProtection3pcd, {}}},
        {});

    PlatformBrowserTest::SetUp();
  }

  bool OnRequest(
      content::URLLoaderInterceptor::RequestParams* params) override {
    std::string host = params->url_request.url.host();
    std::string path = params->url_request.url.path().substr(1);

    if (host != kTrialEnabledDomain || path != kTrialEnabledTopLevelPath) {
      return false;
    }

    URLLoaderInterceptor::WriteResponse(
        base::StrCat({"HTTP/1.1 200 OK\n", "Content-type: text/html\n",
                      "Origin-Trial: ", k1pDeprecationTrialToken, "\n", "\n"}),
        ("<html><head><title>Trial enabled page with iframe</title></head>"
         "<body>"
         "<iframe id='test'></iframe>"
         "</body></html>"),
        params->client.get());
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(ValidityService1pTrialBrowserTest,
                       RemovesInvalidSettingOnJsCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame(kTrialEnabledSite.host(), "a.test");

  // Create a |TOP_LEVEL_TPCD_TRIAL| setting for |top_level_url| without
  // actually enabling the "TopLevelTpcd" trial.
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  CreateAndVerifyFirstPartyTrialGrant(top_level_url,
                                      /*match_subdomains=*/false);

  // Access third-party cookies via javascript in the iframe, which should cause
  // the setting to be removed.
  ContentSettingChangeObserver setting_observer =
      CreateTopLevelTrialSettingsObserver(top_level_url);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify third-party cookie access is no longer permitted under
  // |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService1pTrialBrowserTest,
                       RemoveInvalidSettingOnNavigationCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  // Navigate to |top_level_url|, which has a blank iframe.
  GURL top_level_url =
      https_server_->GetURL(kTrialEnabledSite.host(), "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, top_level_url));

  // Create a |TOP_LEVEL_TPCD_TRIAL| setting for |top_level_url| without
  // actually enabling the "TopLevelTpcd" trial.
  CreateAndVerifyFirstPartyTrialGrant(top_level_url,
                                      /*match_subdomains=*/false);

  // Navigate the iframe to |iframe_url| to set a cookie via a network response,
  // which should cause the setting to be removed.
  ContentSettingChangeObserver setting_observer =
      CreateTopLevelTrialSettingsObserver(top_level_url);
  GURL iframe_url = https_server_->GetURL(
      "a.test", "/set-cookie?name=value;Secure;SameSite=None");
  ASSERT_TRUE(NavigateIFrameAndWaitForCookieAccess(iframe_url));
  setting_observer.Wait();

  // Verify third-party cookie access is no longer permitted under
  // |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService1pTrialBrowserTest,
                       RemoveInvalidSubdomainMatchingSettingOnCookieAccess) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();

  NavigateToPageWithIFrame("a.test", kTrialEnabledSiteSubdomain.host());

  // Create a subdomain-matching |TOP_LEVEL_TPCD_TRIAL| setting for |grant_url|
  // (which |top_level_url| is a subdomain of) without actually enabling the
  // "TopLevelTpcd" trial.
  GURL top_level_url =
      web_contents->GetPrimaryMainFrame()->GetLastCommittedURL();
  GURL iframe_url = GetIFrame()->GetLastCommittedURL();
  GURL grant_url = GURL(base::StrCat(
      {"https://", kTrialEnabledSite.host(), ":", top_level_url.port()}));

  CreateAndVerifyFirstPartyTrialGrant(grant_url,
                                      /*match_subdomains=*/true);

  // Access cookies via javascript in the iframe, which should cause the setting
  // to be removed.
  ContentSettingChangeObserver setting_observer =
      CreateTopLevelTrialSettingsObserver(top_level_url);
  AccessCookieViaJsIn(web_contents, GetIFrame());
  setting_observer.Wait();

  // Verify third-party cookie access is no longer permitted under
  // |top_level_url| or |grant_url|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(), grant_url,
                                       {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(ValidityService1pTrialBrowserTest,
                       PreserveValidSettings) {
  content::WebContents* web_contents = GetActiveWebContents();
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  GURL top_level_url =
      GURL(kTrialEnabledSite.spec() + kTrialEnabledTopLevelPath);
  GURL iframe_url = https_server_->GetURL("a.test", "/title1.html");
  const std::string kIframeId = "test";

  // Navigate to a |top_level_url| page that returns its origin trial
  // token in its HTTP response headers and has an iframe.
  {
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(top_level_url);

    ASSERT_TRUE(content::NavigateToURL(web_contents, top_level_url));
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));

    setting_observer.Wait();

    // Verify third-party cookie access is now permitted under
    // |top_level_url|.
    ASSERT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                         top_level_url, {}, nullptr),
              CONTENT_SETTING_ALLOW);
    ASSERT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                  GURL(), net::SiteForCookies(), top_level_url, {}, nullptr),
              content_settings::CookieSettingsBase::
                  ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  }

  // Write a third-party cookie from the iframe.
  AccessCookieViaJsIn(web_contents, GetIFrame());

  // Since we can't deterministically wait for the ValidityService to do nothing
  // in response to a third-party cookie access permitted by a valid
  // |TOP_LEVEL_TPCD_TRIAL| content setting, navigate to a different top-level
  // site (with an invalid setting) and trigger a third-party cookie access,
  // then after the invalid setting has been removed, check that the
  // |kTrialEnabledSite| content setting still remains.
  {
    GURL other_top_level_url =
        https_server_->GetURL("different-host.test", "/iframe_blank.html");
    ASSERT_TRUE(content::NavigateToURL(web_contents, other_top_level_url));
    ASSERT_TRUE(
        content::NavigateIframeToURL(web_contents, kIframeId, iframe_url));

    // Create a |TOP_LEVEL_TPCD_TRIAL| setting for |other_top_level_url| without
    // actually enabling the "TopLevelTpcd" trial.
    CreateAndVerifyFirstPartyTrialGrant(other_top_level_url,
                                        /*match_subdomains=*/false);

    // Access cookies via javascript in the iframe, which should cause the
    // setting to be removed.
    ContentSettingChangeObserver setting_observer =
        CreateTopLevelTrialSettingsObserver(other_top_level_url);
    AccessCookieViaJsIn(web_contents, GetIFrame());
    setting_observer.Wait();

    // Verify third-party cookie access is no longer permitted under
    // |other_top_level_url|.
    EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                         other_top_level_url, {}, nullptr),
              CONTENT_SETTING_BLOCK);
  }

  // Verify third-party cookie access is still permitted under
  // |top_level_url|.
  EXPECT_EQ(settings->GetCookieSetting(GURL(), net::SiteForCookies(),
                                       top_level_url, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(GetProfile());
  EXPECT_EQ(settings_map->GetContentSetting(
                top_level_url, GURL(),
                ContentSettingsType::TOP_LEVEL_TPCD_TRIAL, nullptr),
            CONTENT_SETTING_ALLOW);
}
}  // namespace tpcd::trial
