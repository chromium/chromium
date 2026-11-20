// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_provider_mac.h"

#import <AuthenticationServices/AuthenticationServices.h>
#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/cancelable_callback.h"
#import "base/check.h"
#import "base/containers/flat_set.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/functional/callback.h"
#import "base/location.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/bind_post_task.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "base/threading/scoped_blocking_call.h"
#import "base/time/time.h"
#import "base/values.h"
#import "chrome/browser/browser_process.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_entra.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_metadata.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#import "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_prefs_handler.h"
#import "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#import "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#import "chrome/common/pref_names.h"
#import "components/policy/core/common/policy_logger.h"
#import "components/prefs/pref_service.h"
#import "content/public/browser/browser_thread.h"
#import "net/base/apple/http_response_headers_util.h"
#import "net/base/apple/url_conversions.h"
#import "net/http/http_request_headers.h"
#import "net/http/http_response_headers.h"
#import "net/http/http_util.h"
#import "net/url_request/url_request.h"
#import "url/gurl.h"
#import "url/origin.h"

namespace enterprise_auth {

namespace {

using FailureReason = ExtensibleEnterpriseSSOProvider::FailureReason;

// Empty function used to ensure SSOServiceEntraAuthControllerDelegate does not
// get destroyed until the data is fetched.
void OnDataFetched(SSOServiceEntraAuthControllerDelegate*) {
  VLOG_POLICY(2, EXTENSIBLE_SSO) << "[ExtensibleEnterpriseSSO] Deleting "
                                    "SSOServiceEntraAuthControllerDelegate";
}

void RecordMetrics(
    std::unique_ptr<ExtensibleEnterpriseSSOProvider::Metrics> metrics) {
  if (metrics->failure_reason.has_value()) {
    base::UmaHistogramEnumeration(
        "Enterprise.ExtensibleEnterpriseSSO.Failure.Reason",
        *metrics->failure_reason);
  }
  if (metrics->failure_reason == FailureReason::kCapabilityCheckTimedOut) {
    // Only timeouts skip the OS interaction metrics below, because the OS
    // didn't say whether the URL is supported.
    return;
  }

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
    std::unique_ptr<ExtensibleEnterpriseSSOProvider::DelegateResult> result) {
  net::HttpRequestHeaders headers;
  metrics->success = result && result->success;
  if (result && result->success && !result->headers.IsEmpty()) {
    headers = result->headers;
  }
  metrics->end_time = base::Time::Now();
  RecordMetrics(std::move(metrics));
  std::move(callback).Run(std::move(headers));
}

}  // namespace

// The provider is created by the capability check, possibly on a worker, and
// then only used on the UI thread. It is handed off rather than shared, so it
// is never used on two threads at once; Apple documents no thread affinity for
// it. The worker may still release its reference after the hand-off.
struct ExtensibleEnterpriseSSOProvider::AuthProvider {
  NSURL* native_url;
  ASAuthorizationSingleSignOnProvider* provider;
};

ExtensibleEnterpriseSSOProvider::Metrics::Metrics(const std::string& host)
    : host(host), start_time(base::Time::Now()) {}

ExtensibleEnterpriseSSOProvider::Metrics::~Metrics() = default;

ExtensibleEnterpriseSSOProvider::DelegateResult::DelegateResult(
    const std::string& name,
    bool success,
    net::HttpRequestHeaders headers)
    : name(name), success(success), headers(std::move(headers)) {}
ExtensibleEnterpriseSSOProvider::DelegateResult::~DelegateResult() = default;

ExtensibleEnterpriseSSOProvider::ExtensibleEnterpriseSSOProvider()
    : capability_check_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_BLOCKING,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {}

ExtensibleEnterpriseSSOProvider::~ExtensibleEnterpriseSSOProvider() = default;

// static
std::unique_ptr<ExtensibleEnterpriseSSOProvider::AuthProvider>
ExtensibleEnterpriseSSOProvider::GetAuthProviderSupportingUrl(const GURL& url) {
  NSURL* native_url = net::NSURLWithGURL(url);
  if (!native_url) {
    return nullptr;
  }
  ASAuthorizationSingleSignOnProvider* provider =
      [ASAuthorizationSingleSignOnProvider
          authorizationProviderWithIdentityProviderURL:native_url];
  if (!provider.canPerformAuthorization) {
    return nullptr;
  }
  auto auth_provider = std::make_unique<AuthProvider>();
  auth_provider->native_url = native_url;
  auth_provider->provider = provider;
  return auth_provider;
}

bool ExtensibleEnterpriseSSOProvider::SupportsOriginFiltering() {
  return base::FeatureList::IsEnabled(kPingSso) &&
         base::FeatureList::IsEnabled(kExtensibleEnterpriseSsoOriginsFiltering);
}

void ExtensibleEnterpriseSSOProvider::FetchOrigins(
    FetchOriginsCallback on_fetch_complete) {
  CHECK(SupportsOriginFiltering());

  std::vector<url::Origin> origins;
  const std::optional<ExtensibleEnterpriseSSOPrefsHandler::Config>
      current_mdm_config =
          ExtensibleEnterpriseSSOPrefsHandler::GetCurrentConfig();
  if (current_mdm_config.has_value() && current_mdm_config->urls.has_value()) {
    const std::vector<GURL>& urls = *current_mdm_config->urls;
    origins.reserve(urls.size());
    for (const GURL& url : urls) {
      CHECK(url.is_valid());
      origins.push_back(url::Origin::Create(url));
    }
  }

  // De-duplicate origins by constructing a `flat_set`.
  auto origins_ptr = std::make_unique<std::vector<url::Origin>>(
      base::flat_set<url::Origin>(std::move(origins)).extract());
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(on_fetch_complete), std::move(origins_ptr)));
}

void ExtensibleEnterpriseSSOProvider::GetData(
    const GURL& url,
    PlatformAuthProviderManager::GetDataCallback callback) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto metrics = std::make_unique<Metrics>(url.GetHost());

  // TODO(b/564412659): Fail if the `url` does not match any entries from
  // `GetCurrentSsoExtensionConfig()->urls`.

  // The authentication headers only apply to HTTP(S) requests.
  if (!url.SchemeIsHTTPOrHTTPS()) {
    OnAuthorizationDone(std::move(callback), std::move(metrics), nullptr);
    return;
  }

  if (!base::FeatureList::IsEnabled(
          kExtensibleEnterpriseSSOAsyncCapabilityCheck)) {
    OnCanPerformAuthorization(std::move(callback), std::move(metrics),
                              /*timed_out=*/false,
                              GetAuthProviderSupportingUrl(url));
    return;
  }

  // Continues with whichever comes first, the check's reply or the timeout,
  // and ignores the other: `CancelableOnceCallback` invalidates its other
  // forwarder on first run (unlike `SplitOnceCallback`, which crashes if both
  // callbacks run). The blocking OS call can't be cancelled, so a stalled check
  // fails open instead of deferring the navigation.
  using CheckDoneCallback =
      base::CancelableOnceCallback<void(bool, std::unique_ptr<AuthProvider>)>;
  auto check_done = std::make_unique<CheckDoneCallback>(base::BindOnce(
      &ExtensibleEnterpriseSSOProvider::OnCanPerformAuthorization,
      weak_factory_.GetWeakPtr(), std::move(callback), std::move(metrics)));
  base::OnceClosure on_timeout =
      base::BindOnce(check_done->callback(), /*timed_out=*/true, nullptr);
  base::OnceCallback<void(std::unique_ptr<AuthProvider>)> on_reply =
      base::BindOnce(check_done->callback(), /*timed_out=*/false);

  // Owns `check_done` on the UI sequence so both forwarders' weak pointers stay
  // valid until the timeout task runs.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](std::unique_ptr<CheckDoneCallback>,
             base::OnceClosure timeout_closure) {
            std::move(timeout_closure).Run();
          },
          std::move(check_done), std::move(on_timeout)),
      kExtensibleEnterpriseSSOAsyncCapabilityCheckTimeout.Get());

  capability_check_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          [](const GURL& url) {
            base::ScopedBlockingCall scoped_blocking_call(
                FROM_HERE, base::BlockingType::MAY_BLOCK);
            return GetAuthProviderSupportingUrl(url);
          },
          url),
      std::move(on_reply));
}

void ExtensibleEnterpriseSSOProvider::OnCanPerformAuthorization(
    PlatformAuthProviderManager::GetDataCallback callback,
    std::unique_ptr<Metrics> metrics,
    bool timed_out,
    std::unique_ptr<AuthProvider> auth_provider) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (timed_out) {
    VLOG_POLICY(2, EXTENSIBLE_SSO)
        << "[ExtensibleEnterpriseSSO] Capability check timed out; continuing "
           "without SSO headers";
    metrics->failure_reason = FailureReason::kCapabilityCheckTimedOut;
    OnAuthorizationDone(std::move(callback), std::move(metrics), nullptr);
    return;
  }

  metrics->url_is_supported = !!auth_provider;

  if (!auth_provider) {
    metrics->failure_reason = FailureReason::kOsCannotPerformAuthorization;
    OnAuthorizationDone(std::move(callback), std::move(metrics), nullptr);
    return;
  }

  const base::ListValue& supported_idps =
      g_browser_process->local_state()->GetList(
          prefs::kExtensibleEnterpriseSSOEnabledIdps);

  for (const base::Value& idp_value : supported_idps) {
    if (const std::string* idp = idp_value.GetIfString();
        idp && *idp == kMicrosoftIdentityProvider) {
      // Reuse the provider that the capability check created.
      ASAuthorizationSingleSignOnProvider* provider = auth_provider->provider;
      SSOServiceEntraAuthControllerDelegate* delegate =
          [[SSOServiceEntraAuthControllerDelegate alloc]
              initWithAuthorizationSingleSignOnProvider:provider];

      // Pass `delegate` as a callback parameter so that it lives beyond the
      // scope of this function and until the callback is called.
      auto final_callback = base::BindPostTaskToCurrentDefault(
          base::BindOnce(&OnAuthorizationDone, std::move(callback),
                         std::move(metrics))
              .Then(base::BindRepeating(&OnDataFetched, delegate)));
      [delegate getAuthHeaders:auth_provider->native_url
                  withCallback:std::move(final_callback)];
      return;
    }
  }

  // Reached when `kMicrosoftIdentityProvider` is excluded from
  // `kExtensibleEnterpriseSSOEnabledIdps` by `ExtensibleEnterpriseSSOBlocklist`
  // (e.g. while another IdP keeps the manager enabled). Skipping a
  // policy-disabled IdP is not recorded in `Failure.Reason`.
  OnAuthorizationDone(std::move(callback), std::move(metrics), nullptr);
}

}  // namespace enterprise_auth
