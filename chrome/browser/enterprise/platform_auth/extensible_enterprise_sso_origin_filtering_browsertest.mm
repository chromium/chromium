// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/base/apple/url_conversions.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace enterprise_auth {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pointee;
using ::testing::UnorderedElementsAre;

using RawConfig = CFPreferencesObserver::RawConfig;
using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;

constexpr char kIdpHost[] = "idp.example";
constexpr char kSecondIdpHost[] = "second-idp.example";
constexpr char kUnrelatedHost[] = "unrelated.example";

// A different path on the configured origin. Served by the default `/echo`
// handler so the navigation commits successfully.
constexpr char kOtherPath[] = "/echo";

constexpr char kNotSupportedHistogram[] =
    "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration";

}  // namespace

// End-to-end tests for origin filtering of `ExtensibleEnterpriseSSOProvider`.
// Only the OS boundary is faked: the MDM payload is served by
// `FakeCFPreferencesObserver` and `ASAuthorizationSingleSignOnProvider` is
// mocked to record the URLs the provider is asked to authenticate.
class ExtensibleEnterpriseSsoOriginFilteringBrowserTest
    : public InProcessBrowserTest {
 protected:
  ExtensibleEnterpriseSsoOriginFilteringBrowserTest()
      : cf_prefs_override_(base::BindRepeating(
            &ExtensibleEnterpriseSsoOriginFilteringBrowserTest::
                CreateCFPrefsObserver,
            base::Unretained(this))) {}

  // InProcessBrowserTest:
  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Make sure no traffic goes out into the real network.
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetCertHostnames({kIdpHost, kSecondIdpHost, kUnrelatedHost});
    net::test_server::RegisterDefaultHandlers(&https_server_);
    ASSERT_TRUE(https_server_.Start());

    MockSsoProvider();

    // Waits for the config read triggered on startup to be applied.
    content::RunAllTasksUntilIdle();

    // The config URLs depend on the test server's port, so they can only be
    // set once the server is running.
    SetConfigUrlsAndWait(std::vector<std::string>{IdpUrl(kIdpHost)});
  }

  void TearDownOnMainThread() override {
    [mock_sso_provider_ stopMocking];
    mock_sso_provider_ = nil;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  std::unique_ptr<CFPreferencesObserver> CreateCFPrefsObserver() {
    return std::make_unique<FakeCFPreferencesObserver>(
        config_update_callback_,
        base::BindRepeating(
            &ExtensibleEnterpriseSsoOriginFilteringBrowserTest::GetConfig,
            base::Unretained(this)));
  }

  // Runs on a background sequence when the prefs handler reads the config.
  RawConfig GetConfig() {
    if (!config_urls_.has_value()) {
      return RawConfig();
    }
    return RawConfig{
        .extension_id = base::SysUTF8ToCFStringRef(kPingSsoExtensionId),
        .team_id = base::SysUTF8ToCFStringRef(kPingSsoTeamId),
        .hosts = ScopedPropList(),
        .urls = StringsToPropList(config_urls_.value()),
    };
  }

  // Stubs `ASAuthorizationSingleSignOnProvider` so that every URL the real
  // provider is asked to authenticate is recorded in `requested_urls_`. The
  // mock reports that it can't perform authorization, so `GetData()` completes
  // with empty headers and the navigation proceeds.
  void MockSsoProvider() {
    mock_sso_provider_ =
        OCMClassMock([ASAuthorizationSingleSignOnProvider class]);
    // Captured unretained to avoid a retain cycle between the mock and its stub
    // block; `mock_sso_provider_` keeps the mock alive until teardown.
    __unsafe_unretained id mock_sso_provider = mock_sso_provider_;
    std::vector<GURL>* requested_urls = &requested_urls_;
    OCMStub([mock_sso_provider_
                authorizationProviderWithIdentityProviderURL:[OCMArg any]])
        .andDo(^(NSInvocation* invocation) {
          __unsafe_unretained NSURL* url = nil;
          [invocation getArgument:&url atIndex:2];
          requested_urls->push_back(net::GURLWithNSURL(url));
          __unsafe_unretained id result = mock_sso_provider;
          [invocation setReturnValue:&result];
        });
    OCMStub([mock_sso_provider_ canPerformAuthorization]).andReturn(NO);
  }

  // Updates the `URLs` entry of the MDM payload and waits until the prefs
  // handler and the manager have applied it. `std::nullopt` removes the whole
  // payload.
  void SetConfigUrlsAndWait(std::optional<std::vector<std::string>> urls) {
    config_urls_ = std::move(urls);
    ASSERT_TRUE(config_update_callback_);
    config_update_callback_.Run();
    content::RunAllTasksUntilIdle();
  }

  void SetBlocklistPolicyAndWait(std::initializer_list<std::string> idps) {
    base::ListValue blocklist;
    for (const std::string& idp : idps) {
      blocklist.Append(idp);
    }
    policy::PolicyMap policies;
    policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(std::move(blocklist)), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
    content::RunAllTasksUntilIdle();
  }

  std::string IdpUrl(const char* host) {
    return https_server_.GetURL(host, "/sso").spec();
  }

  // Navigates the active tab to `url` and expects `expected_commit_url` to
  // commit.
  void NavigateAndExpectCommit(const GURL& url,
                               const GURL& expected_commit_url) {
    ASSERT_TRUE(content::NavigateToURL(
        browser()->GetTabStripModel()->GetActiveWebContents(), url,
        expected_commit_url));
  }

  void NavigateAndExpectCommit(const GURL& url) {
    NavigateAndExpectCommit(url, url);
  }

  static bool IsManagerEnabled() {
    return PlatformAuthProviderManager::GetInstance().IsEnabled();
  }

  std::optional<std::vector<std::string>> config_urls_;
  base::RepeatingClosure config_update_callback_;
  std::vector<GURL> requested_urls_;
  id mock_sso_provider_ = nil;

  base::test::ScopedFeatureList feature_list_{{kPingSso}};
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::ScopedManagementServiceOverrideForTesting platform_management_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL};
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  const ScopedCFPreferenceObserverOverride cf_prefs_override_;
};

// Tests that a navigation to a configured origin reaches the provider, even if
// the path differs from the configured URL.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       ProviderCalledForConfiguredOrigin) {
  base::HistogramTester histogram_tester;
  const GURL url = https_server_.GetURL(kIdpHost, kOtherPath);

  NavigateAndExpectCommit(url);

  EXPECT_THAT(requested_urls_, ElementsAre(url));
  histogram_tester.ExpectTotalCount(kNotSupportedHistogram, 1);
}

// Tests that a navigation to an origin missing from the config never reaches
// the provider.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       ProviderNotCalledForUnconfiguredOrigin) {
  base::HistogramTester histogram_tester;

  NavigateAndExpectCommit(https_server_.GetURL(kUnrelatedHost, kOtherPath));

  EXPECT_THAT(requested_urls_, IsEmpty());
  histogram_tester.ExpectTotalCount(kNotSupportedHistogram, 0);
}

// Tests that a redirect from an unconfigured origin to a configured one only
// reaches the provider for the configured hop.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       RedirectToConfiguredOrigin) {
  const GURL idp_url = https_server_.GetURL(kIdpHost, kOtherPath);
  const GURL redirect_url = https_server_.GetURL(
      kUnrelatedHost, base::StrCat({"/server-redirect", "?", idp_url.spec()}));

  NavigateAndExpectCommit(redirect_url, idp_url);

  EXPECT_THAT(requested_urls_, ElementsAre(idp_url));
}

// Tests that the filtered origins follow runtime changes of the config's URLs.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       OriginsFollowConfigUpdates) {
  const GURL first_idp_url = https_server_.GetURL(kIdpHost, kOtherPath);
  const GURL second_idp_url = https_server_.GetURL(kSecondIdpHost, kOtherPath);

  SetConfigUrlsAndWait(std::vector<std::string>{IdpUrl(kSecondIdpHost)});
  ASSERT_TRUE(IsManagerEnabled());

  NavigateAndExpectCommit(first_idp_url);
  NavigateAndExpectCommit(second_idp_url);

  EXPECT_THAT(requested_urls_, ElementsAre(second_idp_url));
}

// Tests that several configured origins are all forwarded to the provider.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       MultipleConfiguredOrigins) {
  const GURL first_idp_url = https_server_.GetURL(kIdpHost, kOtherPath);
  const GURL second_idp_url = https_server_.GetURL(kSecondIdpHost, kOtherPath);
  const GURL unrelated_url = https_server_.GetURL(kUnrelatedHost, kOtherPath);

  SetConfigUrlsAndWait(
      std::vector<std::string>{IdpUrl(kIdpHost), IdpUrl(kSecondIdpHost)});

  NavigateAndExpectCommit(first_idp_url);
  NavigateAndExpectCommit(unrelated_url);
  NavigateAndExpectCommit(second_idp_url);

  EXPECT_THAT(requested_urls_, ElementsAre(first_idp_url, second_idp_url));
}

// Tests that the provider reports each configured origin once, even when
// several of the config's URLs share an origin.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       FetchOriginsDeduplicatesOrigins) {
  SetConfigUrlsAndWait(std::vector<std::string>{
      IdpUrl(kIdpHost), https_server_.GetURL(kIdpHost, kOtherPath).spec(),
      IdpUrl(kSecondIdpHost), IdpUrl(kIdpHost)});

  ExtensibleEnterpriseSSOProvider provider;
  base::test::TestFuture<std::unique_ptr<std::vector<url::Origin>>> future;
  provider.FetchOrigins(future.GetCallback());

  EXPECT_THAT(future.Take(),
              Pointee(UnorderedElementsAre(
                  url::Origin::Create(GURL(IdpUrl(kIdpHost))),
                  url::Origin::Create(GURL(IdpUrl(kSecondIdpHost))))));
}

// Tests that removing the MDM payload stops all requests from reaching the
// provider.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       ConfigRemoved) {
  SetConfigUrlsAndWait(std::nullopt);
  ASSERT_FALSE(IsManagerEnabled());

  NavigateAndExpectCommit(https_server_.GetURL(kIdpHost, kOtherPath));

  EXPECT_THAT(requested_urls_, IsEmpty());
}

// Tests that blocking all IdPs by policy stops requests from reaching the
// provider, and that unblocking restores the configured origins. Unblocking
// recreates the prefs handler, so this also covers the config being re-read
// asynchronously after the manager has been re-evaluated.
IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOriginFilteringBrowserTest,
                       BlocklistPolicyAll) {
  const GURL url = https_server_.GetURL(kIdpHost, kOtherPath);

  SetBlocklistPolicyAndWait({kAllIdentityProviders});
  ASSERT_FALSE(IsManagerEnabled());
  NavigateAndExpectCommit(url);
  EXPECT_THAT(requested_urls_, IsEmpty());

  SetBlocklistPolicyAndWait({});
  ASSERT_TRUE(IsManagerEnabled());
  NavigateAndExpectCommit(url);
  EXPECT_THAT(requested_urls_, ElementsAre(url));
}

class ExtensibleEnterpriseSsoOriginFilteringDisabledBrowserTest
    : public ExtensibleEnterpriseSsoOriginFilteringBrowserTest {
 protected:
  ExtensibleEnterpriseSsoOriginFilteringDisabledBrowserTest() {
    disabled_feature_list_.InitAndDisableFeature(
        kExtensibleEnterpriseSsoOriginsFiltering);
  }

  base::test::ScopedFeatureList disabled_feature_list_;
};

// Tests that, while `kExtensibleEnterpriseSsoOriginsFiltering` is disabled,
// requests to both configured and unconfigured origins reach the provider.
IN_PROC_BROWSER_TEST_F(
    ExtensibleEnterpriseSsoOriginFilteringDisabledBrowserTest,
    ProviderCalledForAllOrigins) {
  const GURL idp_url = https_server_.GetURL(kIdpHost, kOtherPath);
  const GURL unrelated_url = https_server_.GetURL(kUnrelatedHost, kOtherPath);
  ASSERT_TRUE(IsManagerEnabled());

  NavigateAndExpectCommit(idp_url);
  NavigateAndExpectCommit(unrelated_url);

  EXPECT_THAT(requested_urls_, ElementsAre(idp_url, unrelated_url));
}

}  // namespace enterprise_auth
