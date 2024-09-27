// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_access_api/storage_access_header_service.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
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
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/origin_trial_status_change_details.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
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
constexpr char kRetryPath[] = "/retry-with-storage-access";
constexpr char kHeaderNotProvidedSentinel[] = "HEADER_NOT_PROVIDED";

// Following tokens generated with the instructions at:
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/origin_trials_integration.md#manual-testing.

// Origin Trials token for `kDomainEnabledForTrial` generated with:
// tools/origin_trials/generate_token.py  https://a.test:46665
// StorageAccessHeader --expire-days 1000
inline constexpr char kDomainOriginTrialToken[] =
    "A1LfirZi2W43O+A6E7T0NFxW2fNlycoDlFk3O/"
    "Xbj9FrIuUIImOlZkLbjQPiEFiIahMXzGkLqzuA9rwBCCKG6QAAAABaeyJvcmlnaW4iOiAiaHR0"
    "cHM6Ly9hLnRlc3Q6NDY2NjUiLCAiZmVhdHVyZSI6ICJTdG9yYWdlQWNjZXNzSGVhZGVyIiwgIm"
    "V4cGlyeSI6IDE4MTMwMzI2MTl9";

// Origin Trials token for `kDomainEnabledForTrial` and all its subdomains
// generated with: tools/origin_trials/generate_token.py  https://a.test:46665
// StorageAccessHeader --is-subdomain --expire-days 1000
inline constexpr char kSubdomainMatchingOriginTrialToken[] =
    "A4a8ArZUbH8IX5oFKyt0grhKgr+dM7KBvRnC8hFjR71ktyapbLD8+DUhh+kffHN+"
    "Zp1R2qoBlpJpfac3WTaCTg4AAABveyJvcmlnaW4iOiAiaHR0cHM6Ly9hLnRlc3Q6NDY2NjUiLC"
    "AiZmVhdHVyZSI6ICJTdG9yYWdlQWNjZXNzSGVhZGVyIiwgImV4cGlyeSI6IDE4MTMwMzI2ODIs"
    "ICJpc1N1YmRvbWFpbiI6IHRydWV9";

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
                                network::features::kStorageAccessHeadersTrial},
                               {});
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data/")));
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          return HandleRetryRequest(retry_path_fetch_count_, request);
        }));
    https_server_.RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          most_recent_request_headers_ = request.headers;
        }));
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

    prompt_factory_ =
        std::make_unique<permissions::MockPermissionPromptFactory>(
            permissions::PermissionRequestManager::FromWebContents(
                GetActiveWebContents()));
    // Don't respond to the prompt at all, by default.
    prompt_factory_->set_response_type(
        permissions::PermissionRequestManager::NONE);

    // We use a URLLoaderInterceptor to intercept the loads needed to
    // enable the origin trials for testing.
    url_loader_interceptor_ =
        std::make_unique<URLLoaderInterceptor>(base::BindLambdaForTesting(
            [this](URLLoaderInterceptor::RequestParams* params) {
              return OnRequest(params);
            }));

    // Block third party cookies in all tests.
    GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            content_settings::CookieControlsMode::kBlockThirdParty));
  }

  void TearDownOnMainThread() override {
    url_loader_interceptor_.reset();
    prompt_factory_.reset();
    PlatformBrowserTest::TearDownOnMainThread();
  }

  void NavigateFrameTo(const GURL& url) {
    EXPECT_TRUE(NavigateIframeToURL(GetActiveWebContents(), "test", url));
  }

  GURL GetURL(std::string_view host, std::string_view path = "/") {
    return https_server_.GetURL(host, path);
  }

  void EnsureUserInteractionOn(std::string_view host,
                               Browser* browser_ptr = nullptr) {
    if (browser_ptr == nullptr) {
      browser_ptr = browser();
    }
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser_ptr, GetURL(std::string(host), "/empty.html")));
    ASSERT_TRUE(content::ExecJs(GetActiveWebContents(), ""));
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  net::test_server::HttpRequest::HeaderMap MostRecentRequestHeaders() {
    return most_recent_request_headers_;
  }

 private:
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

  // Needed to test the retry flow of SAA Headers.
  std::unique_ptr<net::test_server::HttpResponse> HandleRetryRequest(
      int& fetch_count,
      const net::test_server::HttpRequest& request) {
    if (request.GetURL().path_piece() != kRetryPath) {
      return nullptr;
    }

    fetch_count++;
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content_type("text/plain");
    http_response->AddCustomHeader("Activate-Storage-Access",
                                   "retry; allowed-origin=*");

    auto lookup_header_value =
        [&](std::string_view header_name) -> std::string {
      std::string value = kHeaderNotProvidedSentinel;
      if (auto it = request.headers.find(header_name);
          it != request.headers.end()) {
        value = it->second;
      }
      return base::JoinString({header_name, value}, ":");
    };

    http_response->set_content(
        lookup_header_value(net::HttpRequestHeaders::kSecFetchStorageAccess));

    return http_response;
  }

  int retry_path_fetch_count_ = 0;
  base::test::ScopedFeatureList features_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<URLLoaderInterceptor> url_loader_interceptor_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  net::test_server::HttpRequest::HeaderMap most_recent_request_headers_;
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

// TODO(crbug.com/367785337): Failing on multiple platforms.
// This test has experienced the most issues with failures relating to
// the call to `https_server_.Start(/*port=*/46665)`, so it should remain
// disabled until we have an alternative to starting the test server on a
// predetermined port for tests involving Origin Trial tokens.
IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       DISABLED_TrialToken_SettingRemovedWhenNoHeader) {
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

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialHeader_CreatesRequestHeader) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));
  NavigateFrameTo(
      GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
  // Subsequent requests should include the header.
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));
  EXPECT_THAT(MostRecentRequestHeaders(),
              testing::Contains(testing::Pair(
                  net::HttpRequestHeaders::kSecFetchStorageAccess, "none")));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialHeader_WithInactiveCase) {
  EnsureUserInteractionOn(kDomainEnabledForTrial);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

  NavigateFrameTo(
      GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)));
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));
  EXPECT_THAT(
      MostRecentRequestHeaders(),
      testing::Contains(testing::Pair(
          net::HttpRequestHeaders::kSecFetchStorageAccess, "inactive")));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialHeader_WithRetryToActive) {
  EnsureUserInteractionOn(kDomainEnabledForTrial);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

  NavigateFrameTo(
      GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      content::ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)));
  NavigateFrameTo(GetURL(kDomainEnabledForTrial, kRetryPath));
  EXPECT_THAT(MostRecentRequestHeaders(),
              testing::Contains(testing::Pair(
                  net::HttpRequestHeaders::kSecFetchStorageAccess, "active")));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialHeader_LoadTokenIsSupported) {
  EnsureUserInteractionOn(kDomainEnabledForTrial);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  content::WebContents* web_contents = GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe_blank.html");
  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

  NavigateFrameTo(
      GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)));

  NavigateFrameTo(GetURL(kDomainEnabledForTrial,
                         "/set-header?Activate-Storage-Access: load"));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(
      ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0)));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeaderServiceBrowserTest,
                       TrialHeader_LosesHeaderWithoutToken) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL embedding_site = GetURL("b.test", "/iframe.html");

  ASSERT_TRUE(content::NavigateToURL(web_contents, embedding_site));

  NavigateFrameTo(
      GetURL(kDomainEnabledForTrial, {base::StrCat({"/", kIframePath})}));
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));
  ASSERT_THAT(MostRecentRequestHeaders(),
              testing::Contains(testing::Pair(
                  net::HttpRequestHeaders::kSecFetchStorageAccess, "none")));
  // The previous navigation did not contain an OT token, subsequent navigations
  // should not contain the header.
  NavigateFrameTo(GetURL(kDomainEnabledForTrial));
  EXPECT_THAT(MostRecentRequestHeaders(),
              testing::Not(testing::Contains(testing::Key(
                  net::HttpRequestHeaders::kSecFetchStorageAccess))));
}
}  // namespace storage_access_api::trial
