// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tpcd/support/trial_test_utils.h"
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

// Origin Trials token for `kTrialEnabledSite` generated with:
// tools/origin_trials/generate_token.py  https://example.test TopLevelTpcd
// --expire-days 5000
const char kTopLevelTrialToken[] =
    "A5sGfiy3qkhJES3yFHkBd7i0jX8rC+"
    "pCA2M0tAhfmetOLkvOVTAR2589eHxZHbdv3QgX7BtANaw3A+"
    "A3NvgAtwIAAABXeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1"
    "cmUiOiAiVG9wTGV2ZWxUcGNkIiwgImV4cGlyeSI6IDIxMzkzMjU5MjZ9";

// Origin Trials token for `kTrialEnabledSite` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://example.test TopLevelTpcd
// --is-subdomain --expire-days 5000
const char kTopLevelTrialSubdomainMatchingToken[] =
    "A5+BZIDRMyQWn2lWBHXWd3egEk2WqNdtEuzEbDZV0qXwYM8nKiqlHNYjGrfXuFgmUQ+"
    "j0wpk0EBVJC51I3K0gQkAAABseyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxlLnRlc3Q6NDQzI"
    "iwgImZlYXR1cmUiOiAiVG9wTGV2ZWxUcGNkIiwgImV4cGlyeSI6IDIxMzkzMzg0NjcsICJpc1N"
    "1YmRvbWFpbiI6IHRydWV9";

// Origin Trials token for `kTrialEnabledSiteSubdomain` generated with:
// tools/origin_trials/generate_token.py  https://sub.example.test TopLevelTpcd
// --expire-days 5000
const char kSubdomainTopLevelTrialToken[] =
    "A7CJlPHXa8yQc2lJRvM/"
    "mq4Oi5+"
    "SJHbT4nnUmWiYKeuguuMkTd6y8DHBRAdEgvLXPajr9Qm2cMe4f5qzovm07QwAAABbeyJvcmlna"
    "W4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIsICJmZWF0dXJlIjogIlRvcExldmV"
    "sVHBjZCIsICJleHBpcnkiOiAyMTM5MzM4NTY5fQ==";

// Origin Trials token for `kTrialEnabledSiteSubdomain` (and all its subdomains)
// generated with:
// tools/origin_trials/generate_token.py https://sub.example.test TopLevelTpcd
// --is-subdomain --expire-days 5000
const char kSubdomainTopLevelTrialSubdomainMatchingToken[] =
    "Ayuwtl4l9AC0MUBPlPDMZ3on5Db2hTQtFJdRM4fC1Bj03JLXWKNoe9bg4m5CslS5wFG9WQQsKu"
    "q/"
    "IbnFBxzGXwMAAABweyJvcmlnaW4iOiAiaHR0cHM6Ly9zdWIuZXhhbXBsZS50ZXN0OjQ0MyIsIC"
    "JmZWF0dXJlIjogIlRvcExldmVsVHBjZCIsICJleHBpcnkiOiAyMTM5MzM4NTIzLCAiaXNTdWJk"
    "b21haW4iOiB0cnVlfQ==";

}  // namespace

class TopLevelTpcdTrialBrowserTest : public PlatformBrowserTest {
 public:
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
    std::string path = params->url_request.url.path().substr(1);
    std::string host = params->url_request.url.host();
    std::string query = params->url_request.url.query();

    if (host != kTrialEnabledDomain && host != kTrialEnabledSubdomain) {
      return false;
    }

    std::string headers =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n";
    std::string body = "";

    std::string token = "";
    if (host == kTrialEnabledDomain) {
      if (query == "subdomain_matching_token") {
        token = kTopLevelTrialSubdomainMatchingToken;
      } else if (query != "no_token") {
        token = kTopLevelTrialToken;
      }
    }

    if (host == kTrialEnabledSubdomain) {
      if (query == "etld_plus_1_token") {
        token = kTopLevelTrialSubdomainMatchingToken;
      } else if (query == "subdomain_matching_token") {
        token = kSubdomainTopLevelTrialSubdomainMatchingToken;
      } else if (query != "no_token") {
        token = kSubdomainTopLevelTrialToken;
      }
    }

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
};

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest, EnabledAfterHttpResponse) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie access isn't permitted under |kTrialEnabledSite|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
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
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
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
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TopLevelTpcdTrialBrowserTest,
                       EnabledAfterHttpResponseWithEtldSubdomainMatchingToken) {
  content::WebContents* web_contents = GetActiveWebContents();

  // Verify third-party cookie access isn't permitted under |kTrialEnabledSite|
  // or |kTrialEnabledSiteSubdomain|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSiteSubdomain, {},
                                       nullptr),
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
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSiteSubdomain, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
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
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSiteSubdomain, {},
                                       nullptr),
            CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
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
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), kTrialEnabledSite, {}, nullptr),
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

  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSiteSubdomain, {},
                                       nullptr),
            CONTENT_SETTING_ALLOW);
  EXPECT_EQ(settings->GetCookieSetting(GURL(), kTrialEnabledSite, {}, nullptr),
            CONTENT_SETTING_ALLOW);

  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), kTrialEnabledSiteSubdomain, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
  EXPECT_EQ(settings->GetThirdPartyCookieAllowMechanism(
                GURL(), kTrialEnabledSite, {}, nullptr),
            content_settings::CookieSettingsBase::
                ThirdPartyCookieAllowMechanism::kAllowByTopLevel3PCD);
}

}  // namespace tpcd::trial
