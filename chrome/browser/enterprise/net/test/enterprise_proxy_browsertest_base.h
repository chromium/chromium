// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {
class EnterpriseProxyErrorService;
class EnterpriseProxyService;
}  // namespace enterprise_net

namespace enterprise::test {

inline constexpr char kTestPvdDomain[] = "pvd.example.com";
inline constexpr char kDestinationHost[] = "destination.example.com";
inline constexpr char kUnmatchedHost[] = "unmatched.foo.com";
inline constexpr char kAuthScope[] = "cloud_secure_gateway";
inline constexpr char kExpectedAccessToken[] = "access_token";

class EnterpriseProxyBrowserTestBase : public MixinBasedPlatformBrowserTest {
 public:
  explicit EnterpriseProxyBrowserTestBase(const ManagementContext& context);
  ~EnterpriseProxyBrowserTestBase() override;

  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_
               ? identity_test_env_adaptor_->identity_test_env()
               : nullptr;
  }

  // Sets the "ProxyProvisioningDomains" policy at the user (cloud) scope.
  void SetUserProxyProvisioningDomains(base::ListValue domains);

  // Sets the "ProxyProvisioningDomains" policy at the machine (device) scope.
  void SetMachineProxyProvisioningDomains(base::ListValue domains);

  base::DictValue CreateDomainPolicyEntry(const std::string& domain_id,
                                          bool use_oauth = false);

  std::string BuildValidPvdJson(const std::string& domain_id,
                                const std::string& proxy_host_port,
                                const std::vector<std::string>& match_domains,
                                bool require_auth = true);

  enterprise_net::EnterpriseProxyService* GetEnterpriseProxyService();
  enterprise_net::EnterpriseProxyErrorService* GetEnterpriseProxyErrorService();
  enterprise::ProfileIdService* GetProfileIdService();

  net::RecordingNetLogObserver& net_log_observer() { return net_log_observer_; }

  void WaitForDynamicRoutesReady();

  // Overrides the HTTP response for the PvD configuration endpoint
  // (/.well-known/pvd) to simulate server errors, 404s, or malformed JSON.
  void SetPvdResponseOverride(
      net::HttpStatusCode code,
      const std::string& content = "",
      const std::string& content_type = "application/json");

  // Configures whether the mock proxy server challenges CONNECT requests with
  // HTTP 407 (Proxy Authentication Required) or allows them directly.
  void SetRequireProxyAuth(bool require_auth);

  // Configures the realm returned in the mock proxy's HTTP 407
  // `Proxy-Authenticate: Basic realm="..."` challenge header (e.g. standard
  // auth realm vs disguised error codes such as "403" or "502").
  void SetProxyChallengeRealm(const std::string& realm);

  bool was_proxy_accessed() const { return was_proxy_accessed_; }
  bool was_auth_header_received() const { return was_auth_header_received_; }
  const std::string& last_received_auth_header() const {
    return last_received_auth_header_;
  }

 protected:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  // Handles proxy CONNECT tunnel requests to `https_server_` when acting as
  // the mock forward proxy, challenging with HTTP 407.
  std::unique_ptr<net::test_server::HttpResponse> HandleAuthRequest(
      const net::test_server::HttpRequest& request);

  // Handles HTTP requests to the PvD configuration endpoint
  // (`/.well-known/pvd`).
  std::unique_ptr<net::test_server::HttpResponse> HandlePvdRequest(
      const net::test_server::HttpRequest& request);

  // Handles HTTP requests to target web pages (`/simple.html` for proxied
  // requests, `/direct.html` for direct bypass requests).
  std::unique_ptr<net::test_server::HttpResponse> HandleWebPageRequest(
      const net::test_server::HttpRequest& request);

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::RecordingNetLogObserver net_log_observer_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  // Subscription to BrowserContextDependencyManager callback used to register
  // testing factory overrides (e.g. IdentityTestEnvironment) before services
  // are created on the profile.
  base::CallbackListSubscription create_services_subscription_;

  // Optional overrides for the PvD HTTP endpoint (/.well-known/pvd) response.
  std::optional<net::HttpStatusCode> pvd_response_override_code_;
  std::string pvd_response_override_content_;
  std::string pvd_response_override_content_type_;

  // Controls whether the mock proxy server challenges CONNECT requests with
  // 407.
  bool require_proxy_auth_ = true;
  // The realm returned in the Proxy-Authenticate header. Can be a normal realm
  // or a disguised error code (e.g. "403", "502").
  std::string proxy_challenge_realm_ = "cloud_secure_gateway";

  // Tracks whether any CONNECT request reached the mock proxy.
  bool was_proxy_accessed_ = false;
  // Tracks whether a Proxy-Authorization header was received by the proxy.
  bool was_auth_header_received_ = false;
  // The value of the last received Proxy-Authorization header.
  std::string last_received_auth_header_;
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
