// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
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

namespace {

const char kTestTokenPublicKey[] =
    "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=,fMS4mpO6buLQ/QMd+zJmxzty/"
    "VQ6B1EUZqoCU04zoRU=";

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

class TpcdSupportBrowserTest : public PlatformBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("origin-trial-public-key",
                                    kTestTokenPublicKey);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        {{::features::kPersistentOriginTrials, {}},
         {net::features::kTpcdSupportSettings, {}},
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
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
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
    if (params->url_request.url == kEnrolledSiteWithToken) {
      // Origin Trials key generated with:
      //
      // tools/origin_trials/generate_token.py --expire-days 5000
      // --is-third-party https://example.test Tpcd
      //
      // Note that you can't have an origin trial token with expiry more than
      // 2^31-1 seconds past the epoch, so (for instance) --expire-days 10000
      // would not have generated a valid token.
      content::URLLoaderInterceptor::WriteResponse(
          base::ReplaceStringPlaceholders(
              "HTTP/1.1 200 OK\n"
              "Content-type: text/html\n"
              "Origin-Trial: $1\n\n",
              {"A1F5vUG256mdaDWxcpAddjWWg7LdOPuoEBswgFVy8b3j0ejT56eJ+e+"
               "IBocST6j2C8nYcnDm6gkd5O7M3FMo4AIAAABPeyJvcmlnaW4iOiAiaHR0cHM6Ly"
               "9leGFtcGxlLnRlc3Q6NDQzIiwgImZlYXR1cmUiOiAiVHBjZCIsICJleHBpcnkiO"
               "iAyMTI0MzA4MDY1fQ=="},
              /*offsets=*/nullptr),
          /*body=*/"", params->client.get());
      return true;
    }
    return false;
  }

  base::test::ScopedFeatureList features_;
  std::unique_ptr<net::EmbeddedTestServer> https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  const GURL kEnrolledSiteWithToken{"https://example.test/with-token"};
};

IN_PROC_BROWSER_TEST_F(TpcdSupportBrowserTest,
                       ThirdPartyIframeEnrolledAfterResponse) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Verify the enrolled site does not have cookie access as a third-party.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(
      settings->GetCookieSetting(kEnrolledSiteWithToken, GURL(), {}, nullptr),
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(settings->GetCookieSetting(kEnrolledSiteWithToken, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_BLOCK);

  // Navigate the top-level page to |embedding_site| and update it to have an
  // iframe pointing to the enrolled site.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kEnrolledSiteWithToken,
        embedding_site, ContentSettingsType::TPCD_SUPPORT);

    ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId,
                                             kEnrolledSiteWithToken));
    setting_observer.Wait();
  }

  // Check that the enrolled site now has access to cookies as a third-party
  // when embedded by |embedding_site|.
  EXPECT_EQ(settings->GetCookieSetting(kEnrolledSiteWithToken, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // TODO (crbug.com/1466156): Actually attempt to read the enrolled site's
  // cookie as a third-party.

  // Check cookie access for |enrolled_site| with a different path and port
  // (since it's generated by |https_server|).
  GURL enrolled_site_diff_path = https_server_->GetURL(
      kEnrolledSiteWithToken.host(), "/iframe_blank.html");

  EXPECT_EQ(settings->GetCookieSetting(enrolled_site_diff_path, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Verify that a subsequent load of a resource from the enrolled site on the
  // embedding site without the token (|enrolled_site_diff_path|) un-enrolls it.
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), enrolled_site_diff_path,
        embedding_site, ContentSettingsType::TPCD_SUPPORT);
    ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId,
                                             enrolled_site_diff_path));
    setting_observer.Wait();
  }

  // Verify the enrolled site no longer has cookie access.
  EXPECT_EQ(settings->GetCookieSetting(kEnrolledSiteWithToken, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(TpcdSupportBrowserTest,
                       EnrolledUsingDifferentSubDomain) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site =
      embedded_test_server()->GetURL("a.test", "/iframe_blank.html");

  // Navigate the top-level page to |embedding_site| and update it to have an
  // iframe pointing to the enrolled site.
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  const std::string kIframeId = "test";  // defined in iframe_blank.html
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(), kEnrolledSiteWithToken,
        embedding_site, ContentSettingsType::TPCD_SUPPORT);

    ASSERT_TRUE(content::NavigateIframeToURL(web_contents, kIframeId,
                                             kEnrolledSiteWithToken));
    setting_observer.Wait();
  }

  // Verify that the enrolled site now has access to cookies as a third-party
  // when embedded by |embedding_site|.
  content_settings::CookieSettings* settings =
      CookieSettingsFactory::GetForProfile(GetProfile()).get();
  EXPECT_EQ(settings->GetCookieSetting(kEnrolledSiteWithToken, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_ALLOW);

  // Check cookie access for a subdomain on |enrolled_site|.
  GURL enrolled_site_subdomain = https_server_->GetURL(
      "sub." + kEnrolledSiteWithToken.host(), "/iframe_blank.html");
  EXPECT_EQ(settings->GetCookieSetting(enrolled_site_subdomain, embedding_site,
                                       {}, nullptr),
            CONTENT_SETTING_ALLOW);
}

// TODO(crbug.com/1466156): add test case(s) for tokens sent during redirects.
