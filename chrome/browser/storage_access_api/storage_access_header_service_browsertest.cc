// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_access_api/storage_access_header_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/content_settings_mock_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"

using content::URLLoaderInterceptor;
using content::WebContents;

namespace {

class ContentSettingChangeObserver : public content_settings::Observer {
 public:
  ContentSettingChangeObserver(content::BrowserContext* browser_context,
                               ContentSettingsPattern primary_pattern,
                               ContentSettingsPattern secondary_pattern)
      : browser_context_(
            raw_ref<content::BrowserContext>::from_ptr((browser_context))),
        primary_pattern_(std::move(primary_pattern)),
        secondary_pattern_(std::move(secondary_pattern)) {
    HostContentSettingsMapFactory::GetForProfile(&*browser_context_)
        ->AddObserver(this);
  }

  ~ContentSettingChangeObserver() override {
    HostContentSettingsMapFactory::GetForProfile(&*browser_context_)
        ->RemoveObserver(this);
  }

  // Waits on a `STORAGE_ACCESS_HEADER_ORIGIN_TRIAL` content
  // setting to be set for the patterns passed to the observer.
  void Wait() { ASSERT_TRUE(future_.Wait()); }

 private:
  // content_settings::Observer overrides:
  void OnContentSettingChanged(
      const ContentSettingsPattern& primary_pattern,
      const ContentSettingsPattern& secondary_pattern,
      ContentSettingsTypeSet content_type_set) override {
    if (content_type_set.Contains(
            ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL) &&
        primary_pattern.Compare(primary_pattern_) ==
            ContentSettingsPattern::IDENTITY &&
        secondary_pattern.Compare(secondary_pattern_) ==
            ContentSettingsPattern::IDENTITY) {
      future_.SetValue();
    }
  }

  raw_ref<content::BrowserContext> browser_context_;
  base::test::TestFuture<void> future_;
  ContentSettingsPattern primary_pattern_;
  ContentSettingsPattern secondary_pattern_;
};

}  // namespace

namespace storage_access_api::trial {

constexpr char kDomainEnabledForTrial[] = "a.test";
constexpr char kIframePath[] = "origin-trial-iframe";
constexpr char kIframePathWithSubdomains[] = "origin-trial-iframe-subdomains";

// Following tokens generated with the instructions at:
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/origin_trials_integration.md#manual-testing.

// Origin Trials token for `kTrialEnabledDomain` generated with:
// tools/origin_trials/generate_token.py  https://a.test:46665
// StorageAccessHeader
inline constexpr char kDomainOriginTrialToken[] =
    "AwDTGouhNig80IdzUHFi+"
    "PLDkDbsjBbdHbqvgdFDze1gE4OY3K2M0VAha54EOcAfIwc4B7J6M6PF+"
    "SUg95VrygUAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly9hLnRlc3Q6NDY2NjUiLCAiZmVhdHVyZSI"
    "6ICJTdG9yYWdlQWNjZXNzSGVhZGVyIiwgImV4cGlyeSI6IDE3MjY2MDA4MzR9";

// Origin Trials token for `kTrialEnabledDomain` and all its subdomains
// generated with: tools/origin_trials/generate_token.py  https://a.test:46665
// StorageAccessHeader --is-subdomain
inline constexpr char kSubdomainMatchingOriginTrialToken[] =
    "A5ee1jtS3b8jqcp2lDC5C1u+"
    "bT129p0NwGCKgPiGAdfjKSx1o5STSgTAtGsVNF3jnXyRty2g9xWZd+"
    "rj5fZJ9Q8AAABveyJvcmlnaW4iOiAiaHR0cHM6Ly9hLnRlc3Q6NDY2NjUiLCAiZmVhdHVyZSI6"
    "ICJTdG9yYWdlQWNjZXNzSGVhZGVyIiwgImV4cGlyeSI6IDE3MjczNjkxOTMsICJpc1N1YmRvbW"
    "FpbiI6IHRydWV9";

class StorageAccessHeaderServiceBrowserTest : public PlatformBrowserTest {
 public:
  StorageAccessHeaderServiceBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }

  void SetUp() override {
    features_.InitWithFeatures({::features::kPersistentOriginTrials,
                                net::features::kStorageAccessHeadersTrial},
                               {});
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data/")));

    // TODO: crbug.com/40860522 - A consistent `port` is currently needed
    // because OT tokens are tied to specific origins and therefore the
    // browsertests in this class need to run with origins containing the same
    // port that the tokens were issued for. Other OT related browsertests are
    // able to avoid this issue because they are able to use a
    // `URLLoaderInterceptor` for all tested navigations; our tests cannot
    // since the StorageAccessHeaders trial changes low-level details in the
    // network stack, which cannot be properly tested if the URL load is
    // intercepted. We should remove the `port` argument below and replace
    // other instances of the predetermined port value (such as in the
    // generation of kDomainOriginTrialToken) with a dynamic port value when
    // dynamic token generation is supported.
    ASSERT_TRUE(https_server_.Start(/*port=*/46665));

    // We use a URLLoaderInterceptor to intercept the loads needed to
    // enable the origin trials for testing.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [this](URLLoaderInterceptor::RequestParams* params) {
              return OnRequest(params);
            }));

    GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

  void NavigateFrameTo(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(GetActiveWebContents(), "test", url));
  }

  GURL GetURL(std::string_view host, std::string_view path = "/") {
    return https_server_.GetURL(host, path);
  }

 protected:
  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  PrefService* GetPrefs() {
    return user_prefs::UserPrefs::Get(
        GetActiveWebContents()->GetBrowserContext());
  }

  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    std::string host = params->url_request.url.host();
    std::string path = params->url_request.url.path().substr(1);

    if ((host != kDomainEnabledForTrial) ||
        (path != kIframePath && path != kIframePathWithSubdomains)) {
      return false;
    }

    std::string headers = base::StrCat({
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n"
        "Origin-Trial: ",
        path == kIframePathWithSubdomains ? kSubdomainMatchingOriginTrialToken
                                          : kDomainOriginTrialToken,
        "\n",
    });

    content::URLLoaderInterceptor::WriteResponse(
        headers,
        /*body=*/"", params->client.get(),
        /*ssl_info=*/std::nullopt,
        /*url=*/params->url_request.url);
    return true;
  }

  base::test::ScopedFeatureList features_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
};

// This tests that no `STORAGE_ACCESS_HEADER_ORIGIN_TRIAL` content settings are
// set without navigating to a page with an OT token.
IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialToken_NoSettingAtALL) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      GetActiveWebContents()->GetBrowserContext());

  content_settings::MockObserver mock_observer;
  absl::Cleanup cleanup = [&] { settings_map->RemoveObserver(&mock_observer); };
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(
                  testing::_, testing::_,
                  ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL))
      .Times(0);
  EXPECT_CALL(
      mock_observer,
      OnContentSettingChanged(
          testing::_, testing::_,
          testing::Ne(ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL)))
      .Times(testing::AnyNumber());
  settings_map->AddObserver(&mock_observer);

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialToken_DefaultsToBlocked) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      GetActiveWebContents()->GetBrowserContext());

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));

  // Since we only include the OT token in the headers when we navigate to the
  // paths `kIframePath` or `kIframePathWithSubdomains`, there should be no OT
  // token in this case. Therefore STORAGE_ACCESS_HEADER_ORIGIN_TRIAL` should
  // default to blocked.
  EXPECT_EQ(
      settings_map->GetContentSetting(
          GetURL(kDomainEnabledForTrial), embedding_site,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_BLOCK);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialToken_HeaderSetsContentSetting) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      GetActiveWebContents()->GetBrowserContext());

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(),
        ContentSettingsPattern::FromURLNoWildcard(
            GetURL(kDomainEnabledForTrial)),
        ContentSettingsPattern::FromURLToSchemefulSitePattern(embedding_site));

    NavigateFrameTo(
        GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
    setting_observer.Wait();
  }

  EXPECT_EQ(
      settings_map->GetContentSetting(
          GetURL(kDomainEnabledForTrial), embedding_site,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialToken_HeaderSetsContentSettingMatchingSubdomains) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      GetActiveWebContents()->GetBrowserContext());

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(),
        ContentSettingsPattern::FromURL(GetURL(kDomainEnabledForTrial)),
        ContentSettingsPattern::FromURLToSchemefulSitePattern(embedding_site));

    NavigateFrameTo(GetURL(kDomainEnabledForTrial,
                           {base::StrCat({"/", kIframePathWithSubdomains})}));
    setting_observer.Wait();
  }

  EXPECT_EQ(
      settings_map->GetContentSetting(
          GetURL(kDomainEnabledForTrial,
                 {base::StrCat({"/", kIframePathWithSubdomains})}),
          embedding_site,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_ALLOW);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialToken_SettingRemovedWhenNoHeader) {
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");
  auto* settings_map = HostContentSettingsMapFactory::GetForProfile(
      GetActiveWebContents()->GetBrowserContext());

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  {
    ContentSettingChangeObserver setting_observer(
        web_contents->GetBrowserContext(),
        ContentSettingsPattern::FromURLNoWildcard(
            GetURL(kDomainEnabledForTrial)),
        ContentSettingsPattern::FromURLToSchemefulSitePattern(embedding_site));

    NavigateFrameTo(
        GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
    setting_observer.Wait();
  }

  // The setting should be removed after a navigation to the origin which does
  // not contain the header.
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));

  EXPECT_EQ(
      settings_map->GetContentSetting(
          GetURL(kDomainEnabledForTrial), embedding_site,
          ContentSettingsType::STORAGE_ACCESS_HEADER_ORIGIN_TRIAL, nullptr),
      CONTENT_SETTING_BLOCK);
}

}  // namespace storage_access_api::trial
