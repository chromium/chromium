// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/run_until.h"
#include "chrome/browser/enterprise/identifiers/profile_id_service_factory.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_error_service_factory.h"
#include "chrome/browser/enterprise/net/enterprise_proxy_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/enterprise/browser/identifiers/profile_id_service.h"
#include "components/enterprise/net/core/enterprise_proxy_error_service.h"
#include "components/enterprise/net/core/enterprise_proxy_service.h"
#include "components/policy/policy_constants.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace enterprise::test {

namespace {

constexpr std::string_view kDefaultAuthSectionJson = R"(
        "auth": {
          "type": "profile_bearer_token",
          "scope": "cloud_secure_gateway"
        },
        "extra_headers": [
          {
            "key": "X-Profile-ID",
            "value": "${profile_id}",
            "type": "variable"
          }
        ]
)";

constexpr char kPvdConfigJsonTemplate[] = R"({
  "identifier": "%s",
  "expires": "Wed, 21 Oct 2026 07:28:00 GMT",
  "proxies": [
    {
      "protocol": "https-connect",
      "identity": "proxy1",
      "proxy": "%s",
      "google_chrome": {
        %s
      }
    }
  ],
  "proxy-match": [
    {
      "proxies": ["proxy1"],
      "domains": [%s]
    }
  ]
})";

const std::vector<std::string>& GetPvdCertHostnames() {
  static const base::NoDestructor<std::vector<std::string>> kHostnames({
      kTestPvdDomain,
      kUnauthenticatedPvdDomain,
      kSecondaryPvdDomain,
      kSeparateSiteHost,
  });
  return *kHostnames;
}

const std::vector<std::string>& GetOriginCertHostnames() {
  static const base::NoDestructor<std::vector<std::string>> kHostnames({
      kDestinationHost,
      kDestinationSubdomainHost,
      kSecondaryDestinationHost,
      kSeparateSiteHost,
      kUnmatchedHost,
  });
  return *kHostnames;
}

}  // namespace

// -----------------------------------------------------------------------------
// EnterpriseProxyBrowserTestBase
// -----------------------------------------------------------------------------

EnterpriseProxyBrowserTestBase::EnterpriseProxyBrowserTestBase(
    const ManagementContext& context)
    : is_platform_managed_(context.is_platform_managed) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{enterprise_net::kEnableDynamicRouteFetching,
                            enterprise_net::kEnterpriseProxyErrorHandling},
      /*disabled_features=*/{});
  management_context_mixin_ =
      ManagementContextMixin::Create(&mixin_host_, this, context);

  pvd_server_.SetCertHostnames(GetPvdCertHostnames());
  pvd_server_.RegisterRequestHandler(
      base::BindRepeating(&EnterpriseProxyBrowserTestBase::HandlePvdRequest,
                          base::Unretained(this)));

  origin_server_.SetCertHostnames(GetOriginCertHostnames());
  origin_server_.RegisterRequestHandler(
      base::BindRepeating(&EnterpriseProxyBrowserTestBase::HandleOriginRequest,
                          base::Unretained(this)));
}

EnterpriseProxyBrowserTestBase::~EnterpriseProxyBrowserTestBase() = default;

void EnterpriseProxyBrowserTestBase::SetUpInProcessBrowserTestFixture() {
  MixinBasedPlatformBrowserTest::SetUpInProcessBrowserTestFixture();
  create_services_subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&EnterpriseProxyBrowserTestBase::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void EnterpriseProxyBrowserTestBase::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
}

void EnterpriseProxyBrowserTestBase::SetUpOnMainThread() {
  MixinBasedPlatformBrowserTest::SetUpOnMainThread();
  host_resolver()->AddRule("*", "127.0.0.1");

  Profile* profile = chrome_test_utils::GetProfile(this);
  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
  AccountInfo account_info =
      identity_test_env_adaptor_->identity_test_env()
          ->MakePrimaryAccountAvailable("user@managed.com",
                                        signin::ConsentLevel::kSignin);
  identity_test_env_adaptor_->identity_test_env()
      ->SimulateSuccessfulFetchOfAccountInfo(
          account_info.GetAccountId(), account_info.GetEmail(),
          account_info.GetGaiaId(),
          /*hosted_domain=*/"managed.com", "Full Name", "Given Name", "en-US",
          /*picture_url=*/"");
  identity_test_env_adaptor_->identity_test_env()
      ->SetAutomaticIssueOfAccessTokens(true);

  ASSERT_TRUE(pvd_server_.InitializeAndListen());
  ASSERT_TRUE(origin_server_.InitializeAndListen());

  default_proxy_server_ = CreateProxyServer(kDefaultProxyHost);
  ASSERT_TRUE(default_proxy_server_);

  pvd_server_.StartAcceptingConnections();
  origin_server_.StartAcceptingConnections();

  scoped_extra_domains_ =
      std::make_unique<enterprise_net::ScopedExtraAllowedDomainsForTesting>(
          std::vector<std::string>{"example.com", "separate-site.com",
                                   "127.0.0.1", "localhost"});
}

void EnterpriseProxyBrowserTestBase::TearDownOnMainThread() {
  scoped_extra_domains_.reset();
  identity_test_env_adaptor_.reset();
  MixinBasedPlatformBrowserTest::TearDownOnMainThread();
  EXPECT_TRUE(pvd_server_.ShutdownAndWaitUntilComplete());
  EXPECT_TRUE(origin_server_.ShutdownAndWaitUntilComplete());
  default_proxy_server_.reset();
}

void EnterpriseProxyBrowserTestBase::SetUserProxyProvisioningDomains(
    base::ListValue domains) {
  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kProxyProvisioningDomains,
                   base::Value(std::move(domains)));
  management_context_mixin_->SetCloudUserPolicies(std::move(policies));
}

void EnterpriseProxyBrowserTestBase::SetMachineProxyProvisioningDomains(
    base::ListValue domains) {
  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kProxyProvisioningDomains,
                   base::Value(std::move(domains)));
  management_context_mixin_->SetCloudMachinePolicies(std::move(policies));
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_ANDROID)
void EnterpriseProxyBrowserTestBase::SetEnableForAllUsers(bool enable) {
  base::flat_map<std::string, std::optional<base::Value>> policies;
  policies.emplace(policy::key::kEnableProxyOverrideRulesForAllUsers,
                   base::Value(enable ? 1 : 0));
  if (is_platform_managed_) {
    management_context_mixin_->SetPlatformPolicies(std::move(policies));
  } else {
    management_context_mixin_->SetCloudMachinePolicies(std::move(policies));
  }
}
#endif

base::DictValue EnterpriseProxyBrowserTestBase::CreateDomainPolicyEntry(
    const std::string& domain_id,
    bool use_oauth,
    base::ListValue extra_headers) {
  base::DictValue entry;
  entry.Set("pvd_id",
            net::HostPortPair(domain_id, pvd_server_.port()).ToString());
  if (use_oauth) {
    base::DictValue auth_config;
    auth_config.Set("type", "profile_bearer_token");
    auth_config.Set("scope", kAuthScope);
    entry.Set("auth_config", std::move(auth_config));
  }
  if (!extra_headers.empty()) {
    entry.Set("extra_headers", std::move(extra_headers));
  }
  return entry;
}

std::string EnterpriseProxyBrowserTestBase::BuildValidPvdJson(
    const std::string& domain_id,
    const std::string& proxy_host_port,
    const std::vector<std::string>& match_domains,
    bool require_auth) {
  std::string domains_json;
  for (size_t i = 0; i < match_domains.size(); ++i) {
    domains_json += base::StringPrintf("\"%s\"", match_domains[i].c_str());
    if (i + 1 < match_domains.size()) {
      domains_json += ", ";
    }
  }

  std::string_view auth_section = require_auth ? kDefaultAuthSectionJson : "";

  return base::StringPrintf(
      kPvdConfigJsonTemplate, domain_id.c_str(), proxy_host_port.c_str(),
      std::string(auth_section).c_str(), domains_json.c_str());
}

enterprise_net::EnterpriseProxyService*
EnterpriseProxyBrowserTestBase::GetEnterpriseProxyService() {
  Profile* profile = chrome_test_utils::GetProfile(this);
  return EnterpriseProxyServiceFactory::GetForProfile(profile);
}

enterprise_net::EnterpriseProxyErrorService*
EnterpriseProxyBrowserTestBase::GetEnterpriseProxyErrorService() {
  Profile* profile = chrome_test_utils::GetProfile(this);
  return EnterpriseProxyErrorServiceFactory::GetForProfile(profile);
}

enterprise::ProfileIdService*
EnterpriseProxyBrowserTestBase::GetProfileIdService() {
  Profile* profile = chrome_test_utils::GetProfile(this);
  return enterprise::ProfileIdServiceFactory::GetForProfile(profile);
}

void EnterpriseProxyBrowserTestBase::WaitForDynamicRoutesReady() {
  auto* service = GetEnterpriseProxyService();
  if (service) {
    ASSERT_TRUE(base::test::RunUntil([service]() {
      return !service->IsRefreshInProgress() &&
             !service->GetDynamicRoutingConfig().routing_rules.empty();
    }));
  }
}

GURL EnterpriseProxyBrowserTestBase::GetOriginURL(std::string_view host,
                                                  std::string_view path) const {
  return origin_server_.GetURL(host, path);
}

GURL EnterpriseProxyBrowserTestBase::GetPvdURL(std::string_view host,
                                               std::string_view path) const {
  return pvd_server_.GetURL(host, path);
}

std::vector<net::HostPortPair>
EnterpriseProxyBrowserTestBase::GetAllProxiedDestinations() const {
  std::vector<net::HostPortPair> destinations;
  for (const std::string& host : GetOriginCertHostnames()) {
    destinations.emplace_back(host, origin_server_.port());
  }
  return destinations;
}

std::unique_ptr<TestConnectProxy>
EnterpriseProxyBrowserTestBase::CreateProxyServer(std::string hostname) {
  auto proxy = std::make_unique<TestConnectProxy>(
      std::move(hostname), GetAllProxiedDestinations(), kAuthScope);
  if (!proxy->Start()) {
    return nullptr;
  }
  return proxy;
}

void EnterpriseProxyBrowserTestBase::SetPvdResponseOverride(
    net::HttpStatusCode code,
    const std::string& content,
    const std::string& content_type) {
  base::AutoLock auto_lock(server_lock_);
  pvd_response_override_code_ = code;
  pvd_response_override_content_ = content;
  pvd_response_override_content_type_ = content_type;
}

void EnterpriseProxyBrowserTestBase::SetRequireProxyAuth(bool require_auth) {
  ASSERT_TRUE(default_proxy_server_);
  default_proxy_server_->SetRequireAuth(require_auth);
}

void EnterpriseProxyBrowserTestBase::SetProxyChallengeRealm(
    const std::string& realm) {
  ASSERT_TRUE(default_proxy_server_);
  default_proxy_server_->SetChallengeRealm(realm);
}

net::test_server::HttpRequest::HeaderMap
EnterpriseProxyBrowserTestBase::last_pvd_request_headers() const {
  base::AutoLock auto_lock(server_lock_);
  return last_pvd_request_headers_;
}

size_t EnterpriseProxyBrowserTestBase::pvd_request_count() const {
  base::AutoLock auto_lock(server_lock_);
  return pvd_request_count_;
}

size_t EnterpriseProxyBrowserTestBase::origin_request_count() const {
  base::AutoLock auto_lock(server_lock_);
  return origin_request_count_;
}

std::unique_ptr<net::test_server::HttpResponse>
EnterpriseProxyBrowserTestBase::HandlePvdRequest(
    const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  if (url.path() != "/.well-known/pvd") {
    return nullptr;
  }

  std::string domain(url.HostNoBrackets());
  if (auto host_it = request.headers.find("Host");
      host_it != request.headers.end()) {
    const GURL host_url(base::StrCat({"https://", host_it->second}));
    if (host_url.is_valid() && !host_url.host().empty()) {
      domain = host_url.HostNoBrackets();
    }
  }
  const std::string default_proxy_host_port =
      default_proxy_server_ ? default_proxy_server_->host_port_pair().ToString()
                            : std::string();

  {
    base::AutoLock auto_lock(server_lock_);
    ++pvd_request_count_;
    last_pvd_request_headers_ = request.headers;

    if (pvd_response_override_code_.has_value()) {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(*pvd_response_override_code_);
      response->set_content(pvd_response_override_content_);
      response->set_content_type(pvd_response_override_content_type_);
      return response;
    }
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content_type("application/json");

  if (domain == kUnauthenticatedPvdDomain) {
    // Matches `kDestinationHost`, but does not require proxy auth.
    response->set_content(
        BuildValidPvdJson(kUnauthenticatedPvdDomain, default_proxy_host_port,
                          {kDestinationHost}, /*require_auth=*/false));
  } else if (domain == kSecondaryPvdDomain) {
    response->set_content(
        BuildValidPvdJson(kSecondaryPvdDomain, default_proxy_host_port,
                          {kSecondaryDestinationHost}, /*require_auth=*/true));
  } else if (domain == kSeparateSiteHost) {
    response->set_content(
        BuildValidPvdJson(kSeparateSiteHost, default_proxy_host_port,
                          {kSeparateSiteHost}, /*require_auth=*/true));
  } else {
    // Default / `kTestPvdDomain`: matches `kDestinationHost` (and its
    // subdomains) with proxy auth.
    response->set_content(BuildValidPvdJson(
        kTestPvdDomain, default_proxy_host_port,
        {kDestinationHost, base::StringPrintf("*.%s", kDestinationHost)},
        /*require_auth=*/true));
  }
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
EnterpriseProxyBrowserTestBase::HandleOriginRequest(
    const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  const std::string_view path = url.path();

  if (path == "/simple.html") {
    {
      base::AutoLock auto_lock(server_lock_);
      ++origin_request_count_;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HttpStatusCode::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(
        "<html><head><link rel=\"icon\" href=\"data:,\"></head>"
        "<body>Proxied and Authenticated</body></html>");
    return response;
  }

  if (path == "/direct.html") {
    {
      base::AutoLock auto_lock(server_lock_);
      ++origin_request_count_;
    }
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HttpStatusCode::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(
        "<html><head><link rel=\"icon\" href=\"data:,\"></head>"
        "<body>Direct Navigation</body></html>");
    return response;
  }

  return nullptr;
}

}  // namespace enterprise::test
