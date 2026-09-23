// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/net/test/enterprise_proxy_browsertest_base.h"

#include <utility>

#include "base/base64.h"
#include "base/no_destructor.h"
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
#include "content/public/browser/browser_thread.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"

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

const std::vector<std::string>& GetTestCertHostnames() {
  static const base::NoDestructor<std::vector<std::string>> kHostnames({
      kTestPvdDomain,
      kDestinationHost,
      kUnmatchedHost,
      "domain1.example.com",
      "domain2.example.com",
  });
  return *kHostnames;
}

}  // namespace

EnterpriseProxyBrowserTestBase::EnterpriseProxyBrowserTestBase(
    const ManagementContext& context) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{enterprise_net::kEnableDynamicRouteFetching,
                            enterprise_net::kEnterpriseProxyErrorHandling},
      /*disabled_features=*/{});
  management_context_mixin_ =
      ManagementContextMixin::Create(&mixin_host_, this, context);
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

  https_server_.SetCertHostnames(GetTestCertHostnames());
  https_server_.RegisterAuthHandler(
      base::BindRepeating(&EnterpriseProxyBrowserTestBase::HandleAuthRequest,
                          base::Unretained(this)));
  https_server_.RegisterRequestHandler(
      base::BindRepeating(&EnterpriseProxyBrowserTestBase::HandlePvdRequest,
                          base::Unretained(this)));
  https_server_.RegisterRequestHandler(
      base::BindRepeating(&EnterpriseProxyBrowserTestBase::HandleWebPageRequest,
                          base::Unretained(this)));

  ASSERT_TRUE(https_server_.InitializeAndListen());
  https_server_.EnableConnectProxy(std::vector<net::HostPortPair>{
      net::HostPortPair(kDestinationHost, https_server_.port()),
  });
  https_server_.StartAcceptingConnections();

  scoped_extra_domains_ =
      std::make_unique<enterprise_net::ScopedExtraAllowedDomainsForTesting>(
          std::vector<std::string>{"example.com", "127.0.0.1", "localhost"});
}

void EnterpriseProxyBrowserTestBase::TearDownOnMainThread() {
  scoped_extra_domains_.reset();
  identity_test_env_adaptor_.reset();
  MixinBasedPlatformBrowserTest::TearDownOnMainThread();
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

base::DictValue EnterpriseProxyBrowserTestBase::CreateDomainPolicyEntry(
    const std::string& domain_id,
    bool use_oauth) {
  base::DictValue entry;
  entry.Set("pvd_id",
            net::HostPortPair(domain_id, https_server_.port()).ToString());
  if (use_oauth) {
    base::DictValue auth_config;
    auth_config.Set("type", "profile_bearer_token");
    auth_config.Set("scope", "cloud_secure_gateway");
    entry.Set("auth_config", std::move(auth_config));
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

void EnterpriseProxyBrowserTestBase::SetPvdResponseOverride(
    net::HttpStatusCode code,
    const std::string& content,
    const std::string& content_type) {
  pvd_response_override_code_ = code;
  pvd_response_override_content_ = content;
  pvd_response_override_content_type_ = content_type;
}

void EnterpriseProxyBrowserTestBase::SetRequireProxyAuth(bool require_auth) {
  require_proxy_auth_ = require_auth;
}

void EnterpriseProxyBrowserTestBase::SetProxyChallengeRealm(
    const std::string& realm) {
  proxy_challenge_realm_ = realm;
}

std::unique_ptr<net::test_server::HttpResponse>
EnterpriseProxyBrowserTestBase::HandleAuthRequest(
    const net::test_server::HttpRequest& request) {
  if (request.method == net::test_server::METHOD_CONNECT) {
    was_proxy_accessed_ = true;
    if (require_proxy_auth_) {
      auto auth_it = request.headers.find("Proxy-Authorization");
      if (auth_it == request.headers.end()) {
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_code(
            net::HttpStatusCode::HTTP_PROXY_AUTHENTICATION_REQUIRED);
        response->AddCustomHeader(
            "Proxy-Authenticate",
            base::StringPrintf("Basic realm=\"%s\"",
                               proxy_challenge_realm_.c_str()));
        response->set_content("Proxy Authentication Required");
        response->set_content_type("text/plain");
        return response;
      }
      was_auth_header_received_ = true;
      last_received_auth_header_ = auth_it->second;
    }
  }
  return nullptr;
}

std::unique_ptr<net::test_server::HttpResponse>
EnterpriseProxyBrowserTestBase::HandlePvdRequest(
    const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  if (url.path() != "/.well-known/pvd") {
    return nullptr;
  }

  if (pvd_response_override_code_.has_value()) {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(*pvd_response_override_code_);
    response->set_content(pvd_response_override_content_);
    response->set_content_type(pvd_response_override_content_type_);
    return response;
  }

  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_code(net::HttpStatusCode::HTTP_OK);
  response->set_content_type("application/json");

  std::string_view domain = url.host();
  if (domain == "domain1.example.com") {
    // Second config also matches kDestinationHost, but does not require auth.
    response->set_content(BuildValidPvdJson(
        "domain1.example.com", https_server_.host_port_pair().ToString(),
        {kDestinationHost}, /*require_auth=*/false));
  } else if (domain == "domain2.example.com") {
    // Third config matches domain2.
    response->set_content(BuildValidPvdJson(
        "domain2.example.com", https_server_.host_port_pair().ToString(),
        {"domain2.example.com"}, /*require_auth=*/true));
  } else {
    // Default / kTestPvdDomain: matches kDestinationHost with auth.
    response->set_content(BuildValidPvdJson(
        kTestPvdDomain, https_server_.host_port_pair().ToString(),
        {kDestinationHost, base::StringPrintf("*.%s", kDestinationHost)},
        /*require_auth=*/true));
  }
  return response;
}

std::unique_ptr<net::test_server::HttpResponse>
EnterpriseProxyBrowserTestBase::HandleWebPageRequest(
    const net::test_server::HttpRequest& request) {
  const GURL url = request.GetURL();
  const std::string_view path = url.path();

  if (path == "/simple.html") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HttpStatusCode::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content(
        "<html><body>Proxied and Authenticated</body></html>");
    return response;
  }

  if (path == "/direct.html") {
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HttpStatusCode::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content("<html><body>Direct Navigation</body></html>");
    return response;
  }

  return nullptr;
}

}  // namespace enterprise::test
