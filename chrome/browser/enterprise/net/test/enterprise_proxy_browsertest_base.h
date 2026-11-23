// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/net/test/test_connect_proxy.h"
#include "chrome/browser/enterprise/test/management_context_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/enterprise/net/core/auth_scope_metadata.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/scoped_extra_allowed_domains_for_testing.h"
#include "components/enterprise/net/core/types.h"
#include "google_apis/gaia/fake_gaia.h"
#include "net/base/host_port_pair.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_event_type.h"
#include "net/log/test_net_log.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
}  // namespace base

namespace enterprise {
class ProfileIdService;
}  // namespace enterprise

namespace enterprise_net {
class EnterpriseProxyErrorService;
class EnterpriseProxyService;
}  // namespace enterprise_net

namespace signin {
class IdentityManager;
}  // namespace signin

namespace enterprise::test {

class TestAuthHelper;

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
inline constexpr char kPorOnlyDestinationHost[] = "por-only.example.com";
inline constexpr char kAddedDestinationHost[] = "added.example.com";
inline constexpr char kRetainedDestinationHost[] = "retained.example.com";
inline constexpr char kRemovedDestinationHost[] = "removed.example.com";
inline constexpr char kUnmatchedHost[] = "unmatched.foo.com";

// CONNECT proxy hostnames served by `TestConnectProxy`.
inline constexpr char kDefaultProxyHost[] = "proxy1.example.com";
inline constexpr char kSecondaryProxyHost[] = "proxy2.example.com";
inline constexpr char kTertiaryProxyHost[] = "proxy3.example.com";

// FakeGaia / enterprise IdP hosts and default managed account constants.
inline constexpr char kGaiaHost[] = "gaia.example.com";
inline constexpr char kIdpHost[] = "idp.example.com";
inline constexpr char kManagedUserEmail[] = "user@managed.com";
inline constexpr char kManagedHostedDomain[] = "managed.com";
inline constexpr char kAuthScope[] = "cloud_secure_gateway";
inline constexpr char kExpectedAccessToken[] = "access_token";

// End-to-end browsertest fixture for Enterprise Proxy (`Secure Gateway`).
// Uses the real `//net` stack with dedicated HTTPS servers (`pvd_server_` with
// deferrable responses, `default_proxy_server_`, and `origin_server_`) and
// realistic auth via `TestAuthHelper`.
class EnterpriseProxyBrowserTestBase : public MixinBasedPlatformBrowserTest {
 public:
  explicit EnterpriseProxyBrowserTestBase(const ManagementContext& context);
  ~EnterpriseProxyBrowserTestBase() override;

  // MixinBasedPlatformBrowserTest:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void SetUpOnMainThread() override;
  void TearDownOnMainThread() override;

  signin::IdentityManager* GetIdentityManager();

  // Signs in a managed primary account via `TestAuthHelper`.
  void SignInPrimaryAccount(
      const std::string& email = kManagedUserEmail,
      const std::string& hosted_domain = kManagedHostedDomain);

  // Invalidates primary account credentials so the next OAuth2 token fetch for
  // `cloud_secure_gateway` fails with `INVALID_GAIA_CREDENTIALS`
  // (`TokenFetchError::kInvalidCredentials` -> `"sign_in_required"`).
  void InvalidatePrimaryAccountCredentials();

  // Restores valid primary account credentials after invalidation so subsequent
  // re-authentication or retry navigations succeed end-to-end.
  void RestorePrimaryAccountCredentials();

  // Evicts any cached OAuth2 access token for `kSecureGatewayService` so the
  // next challenge or authenticated PvD fetch triggers a fresh token request.
  void EvictCachedProxyAccessToken();

  // Returns the number of OAuth2 access token fetch requests issued for
  // `cloud_secure_gateway`.
  size_t GetTokenFetchRequestCount() const;

  // Returns how many times the OS credential-update flow (e.g. Android
  // `AccountManagerFacade.updateCredentials`) was invoked.
  size_t GetUpdateCredentialsCount() const;

  // Sets the "ProxyProvisioningDomains" policy at the user (cloud) scope.
  void SetUserProxyProvisioningDomains(base::ListValue domains);

  // Sets the "ProxyProvisioningDomains" policy at the machine (device) scope.
  void SetMachineProxyProvisioningDomains(base::ListValue domains);

  // Sets the "ProxyOverrideRules" policy at the user (cloud) scope.
  void SetUserProxyOverrideRules(base::ListValue rules);

  // Sets the "ProxySettings" policy at the user (cloud) scope.
  void SetUserProxySettings(base::DictValue settings);

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

  std::string BuildMultiProxyPvdJson(
      const std::string& domain_id,
      const std::vector<std::pair<std::string, std::string>>& proxies,
      const std::vector<std::string>& proxy_order,
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
  GURL GetIdpURL(std::string_view path = "/idp/login") const;

  // Creates and starts an additional HTTPS forward proxy server configured to
  // tunnel to all `origin_server_` destinations.
  std::unique_ptr<TestConnectProxy> CreateProxyServer(
      std::string hostname = kSecondaryProxyHost);

  TestConnectProxy& default_proxy_server() { return *default_proxy_server_; }
  const TestConnectProxy& default_proxy_server() const {
    return *default_proxy_server_;
  }

  // Configures a custom JSON payload for a specific PvD domain on
  // `pvd_server_`.
  void SetPvdJsonForDomain(const std::string& domain, std::string json);

  // Overrides the HTTP response for all `/.well-known/pvd` requests on
  // `pvd_server_` to simulate server errors, 404s, or malformed JSON.
  void SetPvdResponseOverride(
      net::HttpStatusCode code,
      const std::string& content = "",
      const std::string& content_type = "application/json");

  // Controls deferring `/.well-known/pvd` HTTP responses on `pvd_server_` so
  // tests can verify `NetworkService` pausing while `is_update_in_progress` is
  // true across Mojo.
  void SetDeferPvdResponses(bool defer);
  size_t deferred_pvd_request_count() const;
  void WaitForDeferredPvdRequests(size_t expected_count);
  void ReleaseDeferredPvdResponses();

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
  net::test_server::HttpRequest::HeaderMap last_origin_request_headers() const;
  size_t pvd_request_count() const;
  size_t GetPvdRequestCountForDomain(const std::string& domain) const;
  size_t origin_request_count() const;
  size_t idp_page_request_count() const;

 private:
  std::vector<net::HostPortPair> GetAllProxiedDestinations() const;

  std::unique_ptr<net::test_server::HttpResponse> HandlePvdRequest(
      const net::test_server::HttpRequest& request);
  std::unique_ptr<net::test_server::HttpResponse> HandleOriginRequest(
      const net::test_server::HttpRequest& request);

  base::test::ScopedFeatureList scoped_feature_list_;
  // Whether the test's `ManagementContext` simulates OS/platform-level
  // management (selects platform vs. cloud machine policies in
  // `SetEnableForAllUsers()`).
  const bool is_platform_managed_ = false;
  std::unique_ptr<ManagementContextMixin> management_context_mixin_;
  std::unique_ptr<TestAuthHelper> auth_helper_;

  // Captures `NetLog` events (e.g. `ENTERPRISE_PROXY_NETWORK_PAUSE` and
  // `ENTERPRISE_PROXY_AUTH_CHALLENGE_RECEIVED`) for test assertions.
  net::RecordingNetLogObserver net_log_observer_;

  // Active OAuth2 access token served for the signed-in account.
  std::string current_access_token_ = kExpectedAccessToken;

  // Allowlists synthetic test domains for PvD and proxy OAuth token fetches.
  std::unique_ptr<enterprise_net::ScopedExtraAllowedDomainsForTesting>
      scoped_extra_domains_;

  // Guards `pvd_server_` and `origin_server_` override state and request
  // telemetry accessed across `EmbeddedTestServer` IO threads and the main test
  // thread. Declared before the servers below so `~EmbeddedTestServer()` joins
  // its IO thread before `server_lock_` and its guarded fields are destroyed.
  mutable base::Lock server_lock_;

  // When set via `SetPvdResponseOverride()`, `HandlePvdRequest()` returns this
  // status code, body, and `Content-Type` instead of the default PvD JSON.
  std::optional<net::HttpStatusCode> pvd_response_override_code_
      GUARDED_BY(server_lock_);
  std::string pvd_response_override_content_ GUARDED_BY(server_lock_);
  std::string pvd_response_override_content_type_ GUARDED_BY(server_lock_);
  // Per-domain custom PvD JSON payloads set via `SetPvdJsonForDomain()`.
  std::map<std::string, std::string> pvd_json_by_domain_
      GUARDED_BY(server_lock_);
  // Headers from the most recent `/.well-known/pvd` request on `pvd_server_`.
  net::test_server::HttpRequest::HeaderMap last_pvd_request_headers_
      GUARDED_BY(server_lock_);
  // Total `/.well-known/pvd` requests handled by `pvd_server_` (overall and
  // broken down by normalized `Host` header).
  size_t pvd_request_count_ GUARDED_BY(server_lock_) = 0;
  std::map<std::string, size_t> pvd_request_count_by_domain_
      GUARDED_BY(server_lock_);

  // When true, `HandlePvdRequest()` holds `/.well-known/pvd` responses in
  // flight until `ReleaseDeferredPvdResponses()` runs the queued callbacks.
  bool defer_pvd_responses_ GUARDED_BY(server_lock_) = false;
  std::vector<base::OnceClosure> deferred_pvd_release_callbacks_
      GUARDED_BY(server_lock_);

  // Total `/simple.html` and `/direct.html` requests handled by
  // `origin_server_`, and headers from the most recent handled request.
  size_t origin_request_count_ GUARDED_BY(server_lock_) = 0;
  net::test_server::HttpRequest::HeaderMap last_origin_request_headers_
      GUARDED_BY(server_lock_);
  // Total `/idp/login` requests handled by `origin_server_`.
  size_t idp_page_request_count_ GUARDED_BY(server_lock_) = 0;

  FakeGaia fake_gaia_;

  // Separated HTTPS test servers for realistic E2E routing isolation.
  std::unique_ptr<TestConnectProxy> default_proxy_server_;
  net::EmbeddedTestServer origin_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  net::EmbeddedTestServer pvd_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace enterprise::test

#endif  // CHROME_BROWSER_ENTERPRISE_NET_TEST_ENTERPRISE_PROXY_BROWSERTEST_BASE_H_
