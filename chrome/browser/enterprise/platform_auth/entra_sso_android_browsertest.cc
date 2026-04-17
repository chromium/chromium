// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chrome/android/chrome_jni_headers/PlatformAuthEntraTokensReader_jni.h"
#include "chrome/browser/enterprise/platform_auth/entra_provider_android.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/policy_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;
using HeaderMap = std::
    map<std::string, std::string, HttpRequest::CaseInsensitiveStringComparator>;

static constexpr char kInterceptedOrigin[] = "login.microsoftonline.com";

static constexpr auto kAuthTokens =
    base::MakeFixedFlatMap<std::string_view, std::string_view>(
        {{"x-ms-refreshtokencredential", "token0"},
         {"x-ms-refreshtokencredential1", "token1"},
         {"x-ms-refreshtokencredential2", "token2"},
         {"x-ms-devicecredential", "token6"},
         {"x-ms-devicecredential1", "token7"},
         {"x-ms-devicecredential2", "token8"}});

template <typename T>
std::string MakeJsonFromTokens(const T& header_map) {
  base::DictValue root;
  base::DictValue headers;

  for (const auto& [key, value] : header_map) {
    headers.Set(key, value);
  }

  root.Set("headers", std::move(headers));
  std::string json_output;
  base::JSONWriter::Write(root, &json_output);
  return json_output;
}

class JavaTokensReaderResultScopedOverride {
 public:
  JavaTokensReaderResultScopedOverride(
      enterprise_auth::EntraProviderAndroid::Status result_code,
      const std::string& result) {
    JNIEnv* env = base::android::AttachCurrentThread();
    enterprise_auth::
        Java_PlatformAuthEntraTokensReader_setResultOverrideForTesting(
            env, static_cast<int>(result_code), result);
  }

  ~JavaTokensReaderResultScopedOverride() {
    JNIEnv* env = base::android::AttachCurrentThread();
    enterprise_auth::
        Java_PlatformAuthEntraTokensReader_resetResultOverrideForTesting(env);
  }
};

}  // namespace

class EntraSsoAndroidBrowsertest : public PlatformBrowserTest {
 public:
  EntraSsoAndroidBrowsertest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
    SetAndroidEntraSsoEnabledPolicy(1);
    PlatformBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server_.SetCertHostnames({kInterceptedOrigin});
    https_server_.RegisterRequestHandler(base::BindRepeating(
        &EntraSsoAndroidBrowsertest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(https_server_.Start());
  }

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    const GURL url = https_server_.GetURL(request.relative_url);
    if (url.path() != "/login" && url.path() != "/redirect") {
      return nullptr;
    }

    collected_headers_.push_back(request.headers);

    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    if (redirect_callback_) {
      std::move(redirect_callback_).Run();
      http_response->set_code(net::HTTP_FOUND);
      http_response->AddCustomHeader("Location", "/redirect");
    } else {
      http_response->set_code(net::HTTP_OK);
    }

    return http_response;
  }

  net::EmbeddedTestServer https_server_;

  bool NavigateTo(std::string_view origin,
                  std::string_view with_redirect = "") {
    GURL gurl = https_server_.GetURL(origin, "/login");
    enterprise_auth::PlatformAuthProviderManager::GetInstance()
        .GetMutableOriginsForTesting()
        .insert(url::Origin::Create(gurl));
    if (!with_redirect.empty()) {
      GURL final = https_server_.GetURL(origin, with_redirect);
      return content::NavigateToURL(
          chrome_test_utils::GetActiveWebContents(this), gurl, final);
    } else {
      return content::NavigateToURL(
          chrome_test_utils::GetActiveWebContents(this), gurl);
    }
  }

  template <typename MapType>
  void ExpectTokensAttached(const MapType& tokens,
                            const HeaderMap& collected_headers,
                            bool expect_contains) {
    for (const auto& [key, value] : tokens) {
      if (expect_contains) {
        EXPECT_TRUE(collected_headers.contains(key));
      } else {
        EXPECT_FALSE(collected_headers.contains(key));
      }
    }
  }

  void SetAndroidEntraSsoEnabledPolicy(int policy_value) {
    policy::PolicyMap policies;
    policies.Set(policy::key::kAndroidEntraSsoEnabled,
                 policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_MACHINE,
                 policy::POLICY_SOURCE_PLATFORM, base::Value(policy_value),
                 nullptr);
    policy_provider_.UpdateChromePolicy(policies);
  }

  base::test::ScopedFeatureList feature_list_{
      enterprise_auth::kAndroidEntraSSO};

  std::vector<HeaderMap> collected_headers_;
  base::OnceClosure redirect_callback_;
  std::optional<JavaTokensReaderResultScopedOverride> results_override_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;
};

IN_PROC_BROWSER_TEST_F(EntraSsoAndroidBrowsertest,
                       AttachesAuthHeadersForLoginWebsite) {
  results_override_.emplace(enterprise_auth::EntraProviderAndroid::Status::kOk,
                            MakeJsonFromTokens(kAuthTokens));

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin));

  ASSERT_EQ(collected_headers_.size(), 1u);
  const auto& headers = collected_headers_.at(0);
  ExpectTokensAttached(kAuthTokens, headers, true);
}

IN_PROC_BROWSER_TEST_F(EntraSsoAndroidBrowsertest,
                       FetchesFreshHeadersOnEachRedirect) {
  const auto headers_result =
      base::flat_map<std::string_view, std::string_view>(
          {{"x-ms-header-1", "header-value"}});
  results_override_.emplace(enterprise_auth::EntraProviderAndroid::Status::kOk,
                            MakeJsonFromTokens(headers_result));

  redirect_callback_ = base::BindOnce(
      [](std::optional<JavaTokensReaderResultScopedOverride>*
             results_override) {
        results_override->reset();
        const auto fresh_headers_result =
            base::flat_map<std::string_view, std::string_view>(
                {{"x-ms-header-2", "header-value"}});
        results_override->emplace(
            enterprise_auth::EntraProviderAndroid::Status::kOk,
            MakeJsonFromTokens(fresh_headers_result));
      },
      &results_override_);

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin, "/redirect"));

  ASSERT_EQ(collected_headers_.size(), 2u);
  HeaderMap& headers = collected_headers_.at(0);
  EXPECT_TRUE(headers.contains("x-ms-header-1"));
  EXPECT_FALSE(headers.contains("x-ms-header-2"));

  headers = collected_headers_.at(1);
  EXPECT_FALSE(headers.contains("x-ms-header-1"));
  EXPECT_TRUE(headers.contains("x-ms-header-2"));
}

IN_PROC_BROWSER_TEST_F(EntraSsoAndroidBrowsertest, GivesUpAfterFailure) {
  results_override_.emplace(
      enterprise_auth::EntraProviderAndroid::Status::kNoBrokerRegistered,
      MakeJsonFromTokens(kAuthTokens));

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin));

  ASSERT_EQ(collected_headers_.size(), 1u);
  const HeaderMap& headers = collected_headers_.at(0);
  ExpectTokensAttached(kAuthTokens, headers, false);

  results_override_.reset();
  results_override_.emplace(enterprise_auth::EntraProviderAndroid::Status::kOk,
                            MakeJsonFromTokens(kAuthTokens));

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin));

  ASSERT_EQ(collected_headers_.size(), 2u);
  const HeaderMap& new_headers = collected_headers_.at(1);
  ExpectTokensAttached(kAuthTokens, new_headers, false);
}

IN_PROC_BROWSER_TEST_F(EntraSsoAndroidBrowsertest,
                       GivesUpAfterFailureWithRedirect) {
  results_override_.emplace(
      enterprise_auth::EntraProviderAndroid::Status::kNoBrokerRegistered,
      MakeJsonFromTokens(kAuthTokens));

  redirect_callback_ = base::BindOnce(
      [](std::optional<JavaTokensReaderResultScopedOverride>*
             results_override) {
        results_override->reset();
        results_override->emplace(
            enterprise_auth::EntraProviderAndroid::Status::kOk,
            MakeJsonFromTokens(kAuthTokens));
      },
      &results_override_);

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin, "/redirect"));

  ASSERT_EQ(collected_headers_.size(), 2u);
  HeaderMap& headers = collected_headers_.at(0);
  ExpectTokensAttached(kAuthTokens, headers, false);

  headers = collected_headers_.at(1);
  ExpectTokensAttached(kAuthTokens, headers, false);
}

IN_PROC_BROWSER_TEST_F(EntraSsoAndroidBrowsertest, PolicyDisabled) {
  SetAndroidEntraSsoEnabledPolicy(0);

  results_override_.emplace(enterprise_auth::EntraProviderAndroid::Status::kOk,
                            MakeJsonFromTokens(kAuthTokens));

  ASSERT_TRUE(NavigateTo(kInterceptedOrigin));
  ASSERT_EQ(collected_headers_.size(), 1u);
  const auto& headers = collected_headers_.at(0);
  ExpectTokensAttached(kAuthTokens, headers, false);
}
