// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <Foundation/Foundation.h>

#import <string>
#import <utility>
#import <vector>

#import "base/barrier_callback.h"
#import "base/functional/callback.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "chrome/browser/browser_process.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#import "chrome/common/pref_names.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/prefs/pref_service.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "net/url_request/url_request.h"
#import "url/gurl.h"

namespace enterprise_auth {

namespace {

constexpr std::array<const char*, 1> kSupportedIdps{
    kMicrosoftIdentityProvider,
};

// Empty function used to ensure SSOServiceEntraAuthControllerDelegate does not
// get destroyed until the data is fetched.
void OnDataFetched(SSOServiceEntraAuthControllerDelegate*) {
  VLOG_POLICY(2, EXTENSIBLE_SSO) << "[ExtensibleEnterpriseSSO] Deleting "
                                    "SSOServiceEntraAuthControllerDelegate";
}

void RecordMetrics(
    std::unique_ptr<ExtensibleEnterpriseSSOProvider::Metrics> metrics) {
  // TODO(crbug.com/340868357) Check for known hosts.
  if (metrics->url_is_supported) {
    base::UmaHistogramBoolean(
        "Enterprise.ExtensibleEnterpriseSSO.Supported.Result",
        metrics->success);
    base::UmaHistogramTimes(
        metrics->success
            ? "Enterprise.ExtensibleEnterpriseSSO.Supported.Success.Duration"
            : "Enterprise.ExtensibleEnterpriseSSO.Supported.Failure.Duration",
        metrics->end_time - metrics->start_time);
  } else {
    base::UmaHistogramTimes(
        "Enterprise.ExtensibleEnterpriseSSO.NotSupported.Duration",
        metrics->end_time - metrics->start_time);
  }
}

// Function that takes all the possible idp handlers' results and records
// metrics about duration, success and returns the headers of the handler that
// was able to authenticate the request.
void OnAuthorizationDone(
    PlatformAuthProviderManager::GetDataCallback callback,
    std::unique_ptr<ExtensibleEnterpriseSSOProvider::Metrics> metrics,
    std::vector<
        std::unique_ptr<ExtensibleEnterpriseSSOProvider::DelegateResult>>
        results) {
  net::HttpRequestHeaders headers;
  for (const auto& result : results) {
    metrics->success |= result->success;
    if (result->success && !result->headers.IsEmpty()) {
      headers = result->headers;
      break;
    }
  }
  metrics->end_time = base::Time::Now();
  RecordMetrics(std::move(metrics));
  std::move(callback).Run(std::move(headers));
}

}  // namespace

ExtensibleEnterpriseSSOProvider::Metrics::Metrics(const std::string& host)
    : host(host), start_time(base::Time::Now()) {}

ExtensibleEnterpriseSSOProvider::Metrics::~Metrics() = default;

ExtensibleEnterpriseSSOProvider::DelegateResult::DelegateResult(
    const std::string& name,
    bool success,
    net::HttpRequestHeaders headers)
    : name(name), success(success), headers(std::move(headers)) {}
ExtensibleEnterpriseSSOProvider::DelegateResult::~DelegateResult() = default;

ExtensibleEnterpriseSSOProvider::ExtensibleEnterpriseSSOProvider() = default;

ExtensibleEnterpriseSSOProvider::~ExtensibleEnterpriseSSOProvider() = default;

bool ExtensibleEnterpriseSSOProvider::SupportsOriginFiltering() {
  return false;
}

void ExtensibleEnterpriseSSOProvider::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  // Origin filtering is not supported.
  NOTREACHED();
}

void ExtensibleEnterpriseSSOProvider::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  auto metrics = std::make_unique<Metrics>(url.GetHost());
  NSURL* nativeUrl = net::NSURLWithGURL(url);
  ASAuthorizationSingleSignOnProvider* auth_provider =
      [ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:nativeUrl];

  metrics->url_is_supported = auth_provider.canPerformAuthorization;

  if (!auth_provider.canPerformAuthorization) {
    OnAuthorizationDone(std::move(callback), std::move(metrics), {});
    return;
  }

  const base::Value::List& supported_idps =
      g_browser_process->local_state()->GetList(
          prefs::kExtensibleEnterpriseSSOEnabledIdps);

  // Wait for all idps to call this before continuing. This allows to launch all
  // of them in parallel, waiting for all of their results and picking the right
  // ones.
  auto barrier = base::BarrierCallback<
      std::unique_ptr<ExtensibleEnterpriseSSOProvider::DelegateResult>>(
      supported_idps.size(),
      base::BindOnce(&OnAuthorizationDone, std::move(callback),
                     std::move(metrics)));

  for (const base::Value& idp_value : supported_idps) {
    // Setup Microsoft Entra handler
    if (const std::string* idp = idp_value.GetIfString();
        idp && *idp == kMicrosoftIdentityProvider) {
      SSOServiceEntraAuthControllerDelegate* delegate =
          [[SSOServiceEntraAuthControllerDelegate alloc]
              initWithAuthorizationSingleSignOnProvider:auth_provider];

      // Pass `delegate` as a callback parameter so that it lives beyond the
      // scope of this function and until the callback is called.
      auto final_callback = base::BindPostTaskToCurrentDefault(
          barrier.Then(base::BindRepeating(&OnDataFetched, delegate)));
      [delegate getAuthHeaders:nativeUrl
                  withCallback:std::move(final_callback)];
    }
  }
}

// static
std::set<std::string>
ExtensibleEnterpriseSSOProvider::GetSupportedIdentityProviders() {
  return {kSupportedIdps.begin(), kSupportedIdps.end()};
}

// static
base::Value::List
ExtensibleEnterpriseSSOProvider::GetSupportedIdentityProvidersList() {
  base::Value::List idps;
  for (const char* idp : kSupportedIdps) {
    idps.Append(idp);
  }
  return idps;
}

}  // namespace enterprise_auth
