// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/safe_browsing_policy_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class NetworkTimePolicyTest : public SafeBrowsingPolicyTest {
 public:
  NetworkTimePolicyTest() {
    std::map<std::string, std::string> parameters;
    parameters["FetchBehavior"] = "on-demand-only";
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        network_time::kNetworkTimeServiceQuerying, parameters);
  }
  NetworkTimePolicyTest(const NetworkTimePolicyTest&) = delete;
  NetworkTimePolicyTest& operator=(const NetworkTimePolicyTest&) = delete;
  ~NetworkTimePolicyTest() override = default;

  void SetUpOnMainThread() override {
    SafeBrowsingPolicyTest::SetUpOnMainThread();
  }

  // A request handler that returns a dummy response and counts the number of
  // times it is called.
  std::unique_ptr<net::test_server::HttpResponse> CountingRequestHandler(
      const net::test_server::HttpRequest& request) {
    net::test_server::BasicHttpResponse* response =
        new net::test_server::BasicHttpResponse();
    num_requests_++;
    response->set_code(net::HTTP_OK);
    response->set_content(R"(
        )]}'
        {
          "current_time_millis": 1461621971825,
          "server_nonce": -6.006853099049523E85
        })");
    response->AddCustomHeader("x-cup-server-proof", "dead:beef");
    return std::unique_ptr<net::test_server::HttpResponse>(response);
  }

  uint32_t num_requests() { return num_requests_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  uint32_t num_requests_ = 0;
};

IN_PROC_BROWSER_TEST_F(NetworkTimePolicyTest, NetworkTimeQueriesDisabled) {
  // Set a policy to disable network time queries.
  PolicyMap policies;
  policies.Set(key::kBrowserNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(false),
               nullptr);
  UpdateProviderPolicy(policies);

  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      &NetworkTimePolicyTest::CountingRequestHandler, base::Unretained(this)));
  ASSERT_TRUE(embedded_test_server()->Start());
  g_browser_process->network_time_tracker()->SetTimeServerURLForTesting(
      embedded_test_server()->GetURL("/"));

  net::EmbeddedTestServer https_server_expired_(
      net::EmbeddedTestServer::TYPE_HTTPS);
  https_server_expired_.SetSSLConfig(net::EmbeddedTestServer::CERT_EXPIRED);
  ASSERT_TRUE(https_server_expired_.Start());

  // Navigate to a page with a certificate date error and then check that a
  // network time query was not sent.
  ASSERT_TRUE(NavigateToUrl(https_server_expired_.GetURL("/"), this));
  content::WebContents* tab = chrome_test_utils::GetActiveWebContents(this);
  ASSERT_TRUE(IsShowingInterstitial(tab));
  EXPECT_EQ(0u, num_requests());

  // Now enable the policy and check that a network time query is sent.
  policies.Set(key::kBrowserNetworkTimeQueriesEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
               nullptr);
  UpdateProviderPolicy(policies);
  ASSERT_TRUE(NavigateToUrl(https_server_expired_.GetURL("/"), this));
  EXPECT_TRUE(IsShowingInterstitial(tab));
  EXPECT_EQ(1u, num_requests());
}

}  // namespace policy
