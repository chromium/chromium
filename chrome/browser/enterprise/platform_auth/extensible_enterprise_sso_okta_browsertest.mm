// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_policy_observer.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"
#include "chrome/browser/enterprise/platform_auth/scoped_cf_prefs_observer_override.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/enterprise/platform_auth/platform_auth_features.h"
#include "components/enterprise/platform_auth/url_session_test_util.h"
#include "components/enterprise/platform_auth/url_session_url_loader.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/core/common/management/scoped_management_service_override_for_testing.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_host_resolver.h"
#include "net/base/apple/url_conversions.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"

using Config = enterprise_auth::CFPreferencesObserver::Config;
using ScopedPropList = base::apple::ScopedCFTypeRef<CFPropertyListRef>;
using url_session_test_util::ResponseConfig;

namespace {

constexpr char kDomain1[] = "foo.bar.example";
constexpr char kDomain2[] = "example.bar.foo";
constexpr char kDomain3[] = "example.net";

ScopedPropList HostsToPropRef(const std::vector<std::string>& hosts) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> res(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  for (const auto& value : hosts) {
    base::apple::ScopedCFTypeRef<CFStringRef> host =
        base::SysUTF8ToCFStringRef(value);
    CFArrayAppendValue(res.get(), host.get());
  }
  return res;
}

class MockCFPreferencesObserver
    : public enterprise_auth::CFPreferencesObserver {
 public:
  MockCFPreferencesObserver() = default;
  ~MockCFPreferencesObserver() override = default;

  MOCK_METHOD(void, Subscribe, (base::RepeatingClosure on_update), (override));
  MOCK_METHOD(void, Unsubscribe, (), (override));

  MOCK_METHOD(base::OnceCallback<Config()>,
              GetReadConfigCallback,
              (),
              (override));
};

}  // namespace

namespace enterprise_auth {

// These tests simulate the user navigating to a login website, which then
// performs the Okta SSO POST request. Depending on the conditions the request
// should be intercepted and performed using URLSession or left untouched. By
// using a stub URLSession we verify that the request in question indeed
// circumvents Chrome's network stack and is performed using URLSession API.
class ExtensibleEnterpriseSsoOktaBrowserTest : public InProcessBrowserTest {
 public:
  ExtensibleEnterpriseSsoOktaBrowserTest()
      : cf_prefs_override_(
            base::BindRepeating(&ExtensibleEnterpriseSsoOktaBrowserTest::
                                    CreateMockCFPreferenceObserver,
                                base::Unretained(this))) {}

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

    session_override_.emplace();

    PrefService* prefs = g_browser_process->local_state();
    ASSERT_TRUE(prefs);
    platform_auth_policy_observer_.emplace(prefs);

    // Make sure no traffic goes out into the real network.
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &ExtensibleEnterpriseSsoOktaBrowserTest::HandleRequest,
        base::Unretained(this)));
    https_server_.SetCertHostnames({kDomain1, kDomain2, kDomain3});

    ASSERT_TRUE(https_server_.Start());
  }

  void TearDown() override {
    session_override_.reset();
    InProcessBrowserTest::TearDown();
  }

 protected:
  std::unique_ptr<CFPreferencesObserver> CreateMockCFPreferenceObserver() {
    auto mock_cf_observer =
        std::make_unique<testing::NiceMock<MockCFPreferencesObserver>>();

    EXPECT_CALL(*mock_cf_observer, GetReadConfigCallback())
        .WillRepeatedly([this]() {
          return base::BindOnce(
              &ExtensibleEnterpriseSsoOktaBrowserTest::GetConfig,
              base::Unretained(this));
        });

    EXPECT_CALL(*mock_cf_observer, Subscribe(testing::_))
        .WillRepeatedly([this](base::RepeatingClosure callback) {
          config_update_callback_ = std::move(callback);
        });

    EXPECT_CALL(*mock_cf_observer, Unsubscribe).WillRepeatedly([this]() {
      config_update_callback_.Reset();
    });

    return mock_cf_observer;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    GURL absolute_url = https_server_.GetURL(request.relative_url);
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_OK);
    http_response->set_content("<html><body><h1>Login Page</h1></body></html>");
    http_response->set_content_type("text/html");
    return http_response;
  }

  std::string CreateSsoRequest(std::string_view domain) {
    std::string path = enterprise_auth::kOktaSsoURLPattern.Get();

    // Replace all wildcard segments in the path.
    base::ReplaceChars(path, "*", "123", &path);

    const GURL test_gurl = https_server_.GetURL(domain, path);
    return test_gurl.spec();
  }

  void CheckSSORequest(bool expect_response,
                       std::string_view hostname = kDomain1,
                       Browser* target_browser = nullptr) {
    if (!target_browser) {
      target_browser = browser();
    }

    const GURL test_url = https_server_.GetURL(hostname, "/login");
    ASSERT_TRUE(ui_test_utils::NavigateToURL(target_browser, test_url));

    const auto result = content::EvalJs(
        target_browser->tab_strip_model()->GetActiveWebContents(),
        content::JsReplace(
            R"(
            fetch($1, {
              method: 'POST',
              body: $2,
              headers: {
                "Accept": "application/json; okta-version=1.0.0",
                "Cache-Control": "no-cache",
                "Content-Type": "application/json",
                "Pragma": "no-cache",
                "Priority": "u=1, i",
                "X-Okta-User-Agent-Extended": "okta-auth-js/7.14.0 okta-signin-widget-7.37.0",
              }
            })
            .then(response => {
              if (response.ok) {
                return response.json();
              }
              return { "error": "FAILURE", "status": response.status };
            })
            .catch(error => {
                return { "error": "NETWORK_ERROR", "message": error.message };
            });
        )",
            CreateSsoRequest(hostname), url_session_test_util::kTestBody));

    if (expect_response) {
      auto expected_value =
          base::JSONReader::Read(url_session_test_util::kTestBody,
                                 base::JSONParserOptions::JSON_PARSE_RFC);

      ASSERT_TRUE(expected_value.has_value())
          << "kTestServerResponseBody is not valid JSON!";
      EXPECT_EQ(*expected_value, result.ExtractDict());
    } else {
      EXPECT_NE(result, *base::JSONReader::Read(
                            url_session_test_util::kTestBody,
                            base::JSONParserOptions::JSON_PARSE_RFC));

      const base::DictValue& result_dict = result.ExtractDict();
      EXPECT_TRUE(result_dict.Find("error"));
    }
  }

  void SetBlocklistPolicy(std::initializer_list<std::string> idps) {
    policy::PolicyMap policies;
    base::ListValue blocklist;

    for (const auto& val : idps) {
      blocklist.Append(val);
    }

    policies.Set(policy::key::kExtensibleEnterpriseSSOBlocklist,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM,
                 base::Value(std::move(blocklist)), nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  Config GetConfig() {
    return Config(
        ScopedPropList(
            ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID),
        ScopedPropList(ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID),
        HostsToPropRef(configured_hosts_));
  }

  std::vector<std::string> configured_hosts_{kDomain1};
  base::RepeatingClosure config_update_callback_;
  base::test::ScopedFeatureList feature_list_{enterprise_auth::kOktaSSO};
  std::optional<PlatformAuthPolicyObserver> platform_auth_policy_observer_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
  policy::ScopedManagementServiceOverrideForTesting platform_management_{
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::COMPUTER_LOCAL};

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::optional<ProxyingURLLoaderFactory::ScopedURLSessionOverrideForTesting>
      session_override_;
  const ScopedCFPreferenceObserverOverride cf_prefs_override_;
};

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest, Successful) {
  // By default the test setup should make the SSO work.
  CheckSSORequest(true);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       DoesNotProxyInIncognito) {
  // Create an Incognito browser window linked to the main profile.
  Browser* incognito_browser = CreateIncognitoBrowser(browser()->profile());
  ASSERT_TRUE(incognito_browser);

  // The SSO proxying should not occur in an Incognito window.
  CheckSSORequest(/*expect_response=*/false, kDomain1, incognito_browser);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       DoesNotProxyInGuestMode) {
  // Create a Guest browser window.
  Browser* guest_browser = CreateGuestBrowser();
  ASSERT_TRUE(guest_browser);

  // The SSO proxying should not occur in a Guest window.
  CheckSSORequest(/*expect_response=*/false, kDomain1, guest_browser);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest, Unmanaged) {
  policy::ScopedManagementServiceOverrideForTesting platform_management(
      policy::ManagementServiceFactory::GetForPlatform(),
      policy::EnterpriseManagementAuthority::NONE);

  // Reset the policy observer.
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);
  platform_auth_policy_observer_.emplace(prefs);

  CheckSSORequest(false);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       BlocklistPolicyOkta) {
  SetBlocklistPolicy({kOktaIdentityProvider});
  CheckSSORequest(false);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       BlocklistPolicyAll) {
  SetBlocklistPolicy({kAllIdentityProviders});
  CheckSSORequest(false);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       BlocklistPolicyEmpty) {
  SetBlocklistPolicy({});
  CheckSSORequest(true);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       BlocklistPolicyEntra) {
  SetBlocklistPolicy({kMicrosoftIdentityProvider});
  CheckSSORequest(true);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       NotConfiguredHost) {
  configured_hosts_ = {};

  // Reset the policy observer to trigger update.
  PrefService* prefs = g_browser_process->local_state();
  ASSERT_TRUE(prefs);
  platform_auth_policy_observer_.emplace(prefs);

  CheckSSORequest(false);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       ListensForConfigChanges) {
  configured_hosts_ = {};
  config_update_callback_.Run();
  base::test::RunUntil([&]() {
    return g_browser_process->local_state()
        ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
        .empty();
  });

  // First the SSO shouldn't work because no hosts are configured.
  CheckSSORequest(false);

  // Update the configured host.
  configured_hosts_ = {kDomain1};
  config_update_callback_.Run();
  base::test::RunUntil([&]() {
    return !g_browser_process->local_state()
                ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
                .empty();
  });

  // Now the configured host is kConfiguredHost and the SSO should work.
  CheckSSORequest(true);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       ConfigUpdateWhileSsoDisabled) {
  configured_hosts_ = {};
  config_update_callback_.Run();
  base::test::RunUntil([&]() {
    return g_browser_process->local_state()
        ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
        .empty();
  });
  CheckSSORequest(false);

  // Disable the SSO by setting the policy.
  SetBlocklistPolicy({kAllIdentityProviders});

  // Fix the configured hosts and enable SSO again.
  configured_hosts_ = {kDomain1};

  // Enable SSO again, this should trigger update of the configured hosts.
  SetBlocklistPolicy({});
  base::test::RunUntil([&]() {
    return !g_browser_process->local_state()
                ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
                .empty();
  });

  CheckSSORequest(true);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       MultipleConfiguredHosts) {
  configured_hosts_ = {kDomain1, kDomain2};
  config_update_callback_.Run();
  base::test::RunUntil([&]() {
    return g_browser_process->local_state()
               ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
               .size() == 2;
  });

  CheckSSORequest(true, kDomain1);
  CheckSSORequest(true, kDomain2);
  CheckSSORequest(false, kDomain3);
}

}  // namespace enterprise_auth
