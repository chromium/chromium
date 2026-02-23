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
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
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

constexpr char kLoginWebsiteDomain[] = "foo.bar.example";

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
    https_server_.SetCertHostnames({kLoginWebsiteDomain});

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
                       std::string_view hostname = kLoginWebsiteDomain,
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
              body: JSON.stringify({data: 'data'})
            })
            .then(response => {
              if (response.ok) {
                return response.text();
              }
              return "FAILURE: " + response.status;
            })
            .catch(error => "NETWORK_ERROR: " + error.message);
        )",
            CreateSsoRequest(hostname)));

    if (expect_response) {
      EXPECT_EQ(URLSessionURLLoader::kTestServerResponseBody, result);
    } else {
      EXPECT_NE(URLSessionURLLoader::kTestServerResponseBody, result);
    }
  }

  Config GetConfig() {
    return Config(
        ScopedPropList(
            ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOExtensionID),
        ScopedPropList(ExtensibleEnterpriseSSOPrefsHandler::kOktaSSOTeamID),
        HostsToPropRef(configured_hosts_));
  }

  std::vector<std::string> configured_hosts_{kLoginWebsiteDomain};
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
  CheckSSORequest(/*expect_response=*/false, kLoginWebsiteDomain,
                  incognito_browser);
}

IN_PROC_BROWSER_TEST_F(ExtensibleEnterpriseSsoOktaBrowserTest,
                       DoesNotProxyInGuestMode) {
  // Create a Guest browser window.
  Browser* guest_browser = CreateGuestBrowser();
  ASSERT_TRUE(guest_browser);

  // The SSO proxying should not occur in a Guest window.
  CheckSSORequest(/*expect_response=*/false, kLoginWebsiteDomain,
                  guest_browser);
}

}  // namespace enterprise_auth
