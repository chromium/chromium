// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/net/test/test_connect_proxy.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/net/core/auth_scope_metadata.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/scoped_extra_allowed_domains_for_testing.h"
#include "components/enterprise/net/core/types.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {
class EnterpriseProxyErrorService;
class EnterpriseProxyService;
}  // namespace enterprise_net

namespace enterprise::test {

// Primary PvD configuration domain; routes `kDestinationHost` (and its
// subdomains) through `default_proxy_server_` with OAuth bearer auth.
inline constexpr char kTestPvdDomain[] = "pvd.example.com";
// Secondary PvD domain that also matches `kDestinationHost`, but without proxy
// auth (used to verify first-matching PvD precedence and multi-PvD merging).
inline constexpr char kUnauthenticatedPvdDomain[] = "unauth-pvd.example.com";
// Secondary PvD domain that routes `kSecondaryDestinationHost` with proxy auth.
inline constexpr char kSecondaryPvdDomain[] = "secondary-pvd.example.com";
// PvD and origin host under a distinct registrable domain (`separate-site.com`
// vs. `example.com`) so navigations get a fresh `NetworkAnonymizationKey` that
// cannot reuse pooled sockets or cached proxy auth from `kDestinationHost`.
inline constexpr char kSeparateSiteHost[] = "destination.separate-site.com";

// Target origin hosts served by `origin_server_`.
inline constexpr char kDestinationHost[] = "destination.example.com";
inline constexpr char kDestinationSubdomainHost[] =
    "sub.destination.example.com";
inline constexpr char kSecondaryDestinationHost[] =
    "secondary-destination.example.com";
inline constexpr char kUnmatchedHost[] = "unmatched.foo.com";

// CONNECT proxy hostnames served by `TestConnectProxy`.
inline constexpr char kDefaultProxyHost[] = "proxy1.example.com";
inline constexpr char kSecondaryProxyHost[] = "proxy2.example.com";

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

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
  void SetEnableForAllUsers(bool enable);
#endif

  base::DictValue CreateDomainPolicyEntry(
      const std::string& domain_id,
      bool use_oauth = false,
      base::ListValue extra_headers = base::ListValue());

  std::string BuildValidPvdJson(const std::string& domain_id,
                                const std::string& proxy_host_port,
                                const std::vector<std::string>& match_domains,
                                bool require_auth = true);

  enterprise_net::EnterpriseProxyService* GetEnterpriseProxyService();
  enterprise_net::EnterpriseProxyErrorService* GetEnterpriseProxyErrorService();
  enterprise::ProfileIdService* GetProfileIdService();

  net::RecordingNetLogObserver& net_log_observer() { return net_log_observer_; }

  void WaitForDynamicRoutesReady();

  // URL builders for the separated test servers.
  GURL GetOriginURL(std::string_view host, std::string_view path) const;
  GURL GetPvdURL(std::string_view host,
                 std::string_view path = "/.well-known/pvd") const;

  // Creates and starts an additional HTTPS forward proxy server configured to
  // tunnel to all `origin_server_` destinations.
  std::unique_ptr<TestConnectProxy> CreateProxyServer(
      std::string hostname = kSecondaryProxyHost);

  TestConnectProxy& default_proxy_server() { return *default_proxy_server_; }
  const TestConnectProxy& default_proxy_server() const {
    return *default_proxy_server_;
  }

  // Overrides the HTTP response for all `/.well-known/pvd` requests on
  // `pvd_server_` to simulate server errors, 404s, or malformed JSON.
  void SetPvdResponseOverride(
      net::HttpStatusCode code,
      const std::string& content = "",
      const std::string& content_type = "application/json");

  // Convenience forwarders to `default_proxy_server()`.
  void SetRequireProxyAuth(bool require_auth);
  void SetProxyChallengeRealm(const std::string& realm);
  bool was_proxy_accessed() const {
    return default_proxy_server_ && default_proxy_server_->was_accessed();
  }
  bool was_auth_header_received() const {
    return default_proxy_server_ &&
           default_proxy_server_->was_auth_header_received();
  }
  std::string last_received_auth_header() const {
    return default_proxy_server_
               ? default_proxy_server_->last_received_auth_header()
               : std::string();
  }

  // Server request telemetry accessors.
  net::test_server::HttpRequest::HeaderMap last_pvd_request_headers() const;
  size_t pvd_request_count() const;
  size_t origin_request_count() const;

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context);
  std::vector<net::HostPortPair> GetAllProxiedDestinations() const;

  // Handles HTTP requests to the PvD configuration endpoint
  // (`/.well-known/pvd`).
  std::unique_ptr<net::test_server::HttpResponse> HandlePvdRequest(
      const net::test_server::HttpRequest& request);

  // Handles HTTP requests to target web pages (`/simple.html` for proxied
  // requests, `/direct.html` for direct bypass requests).
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request);

  base::test::ScopedFeatureList scoped_feature_list_;
  // Whether the test's `ManagementContext` simulates OS/platform-level
  // management (selects platform vs. cloud machine policies in
  // `SetEnableForAllUsers()`).
  const bool is_platform_managed_ = false;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;

  // Captures `NetLog` events (e.g. `ENTERPRISE_PROXY_NETWORK_PAUSE` and
  // `ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED`) for test assertions.
  net::RecordingNetLogObserver net_log_observer_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  // Subscription to BrowserContextDependencyManager callback used to register
  // testing factory overrides (e.g. IdentityTestEnvironment) before services
  // are created on the profile.
  base::CallbackListSubscription create_services_subscription_;

  // Allowlists synthetic test domains for PvD and proxy OAuth token fetches.
  std::unique_ptr<enterprise_net::ScopedExtraAllowedDomainsForTesting>
      scoped_extra_domains_;

  // Guards `pvd_server_` and `origin_server_` override state and request
  // telemetry accessed across `EmbeddedTestServer` IO threads and the main
  // test thread. Declared before the servers below so `~EmbeddedTestServer()`
  // joins its IO thread before `server_lock_` and its guarded fields are
  // destroyed.
  mutable base::Lock server_lock_;
  // When set via `SetPvdResponseOverride()`, `HandlePvdRequest()` returns this
  // status code, body, and `Content-Type` instead of the default PvD JSON.
  std::optional<net::HttpStatusCode> pvd_response_override_code_
      GUARDED_BY(server_lock_);
  std::string pvd_response_override_content_ GUARDED_BY(server_lock_);
  std::string pvd_response_override_content_type_ GUARDED_BY(server_lock_);
  // Headers from the most recent `/.well-known/pvd` request on `pvd_server_`.
  net::test_server::HttpRequest::HeaderMap last_pvd_request_headers_
      GUARDED_BY(server_lock_);
  // Total `/.well-known/pvd` requests handled by `pvd_server_`.
  size_t pvd_request_count_ GUARDED_BY(server_lock_) = 0;
  // Total `/simple.html` and `/direct.html` requests handled by
  // `origin_server_`.
  size_t origin_request_count_ GUARDED_BY(server_lock_) = 0;

  // Separated HTTPS test servers for realistic E2E routing isolation.
  std::unique_ptr<TestConnectProxy> default_proxy_server_;
  net::EmbeddedTestServer origin_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer pvd_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
