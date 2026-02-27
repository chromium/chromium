// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_is_test.h"
#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/platform_auth/platform_auth_features.h"
#include "components/enterprise/platform_auth/url_session_helper.h"
#include "components/enterprise/platform_auth/url_session_url_loader.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_id.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "url/origin.h"

namespace enterprise_auth {

namespace {

void RecordCorsViolationMetric() {
  base::UmaHistogramBoolean(URLSessionURLLoader::kOktaResultHistogram, false);
  base::UmaHistogramEnumeration(
      URLSessionURLLoader::kOktaFailureReasonHistogram,
      URLSessionURLLoader::SSORequestFailReason::kCorsViolation);
}

}  // namespace

ProxyingURLLoaderFactory::ProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    base::flat_set<std::string> configured_hosts,
    const url::Origin& request_initiator)
    : configured_hosts_(std::move(configured_hosts)),
      request_initiator_(request_initiator) {
  DCHECK(!target_factory_.is_bound());
  // base::Unretained here is safe because the callbacks are owned by this, so
  // when this destroys itself, the callbacks will also get destroyed.
  target_factory_.Bind(std::move(target_factory));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&ProxyingURLLoaderFactory::OnTargetFactoryDisconnect,
                     base::Unretained(this)));

  // base::Unretained here is safe for the same reason.
  proxy_receivers_.Add(this, std::move(receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &ProxyingURLLoaderFactory::OnProxyDisconnect, base::Unretained(this)));
}

// static
void ProxyingURLLoaderFactory::MaybeProxyRequest(
    const url::Origin& request_initiator,
    ChromeContentBrowserClient::URLLoaderFactoryType type,
    content::BrowserContext* context,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (context->IsOffTheRecord() ||
      !enterprise_auth::PlatformAuthProviderManager::GetInstance()
           .IsEnabled() ||
      request_initiator.scheme() != url::kHttpsScheme ||
      type != ChromeContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource ||
      !g_browser_process->local_state()
           ->GetList(prefs::kExtensibleEnterpriseSSOEnabledIdps)
           .contains(kOktaIdentityProvider)) {
    return;
  }

  const base::ListValue& configured_hosts_pref =
      g_browser_process->local_state()->GetList(
          prefs::kExtensibleEnterpriseSSOConfiguredHosts);
  if (!configured_hosts_pref.contains(request_initiator.host())) {
    return;
  }

  // Cache configured hosts for a quicker lookup later on.
  base::flat_set<std::string> configured_hosts;
  configured_hosts.reserve(configured_hosts_pref.size());
  for (const base::Value& host : configured_hosts_pref) {
    configured_hosts.insert(host.GetString());
  }
  auto [loader_receiver, target_factory] = factory_builder.Append();
  new ProxyingURLLoaderFactory(std::move(loader_receiver),
                               std::move(target_factory),
                               std::move(configured_hosts), request_initiator);
}

void ProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (ShouldInterceptRequest(request)) {
    if (intercepted_request_callback_for_testing_) {
      std::move(intercepted_request_callback_for_testing_).Run(request);
    } else {
      content::GetNetworkService()->CreateURLSessionURLLoaderAndStart(
          request, std::move(loader_receiver), std::move(client));
    }
  } else {
    target_factory_->CreateLoaderAndStart(
        std::move(loader_receiver), request_id, options, request,
        std::move(client), traffic_annotation);
  }
}

void ProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void ProxyingURLLoaderFactory::OnTargetFactoryDisconnect() {
  delete this;
}

void ProxyingURLLoaderFactory::OnProxyDisconnect() {
  if (proxy_receivers_.empty()) {
    delete this;
  }
}

bool ProxyingURLLoaderFactory::ShouldInterceptRequest(
    const network::ResourceRequest& request) {
  // Only intercept requests to domains configured by the MDM profile.
  if (!configured_hosts_.contains(request.url.host())) {
    return false;
  }

  // Check if this request fits the pattern for the Okta SSO request.
  if (!url_session_helper::IsOktaSSORequest(request)) {
    return false;
  }

  // Make sure the renderer process has set a correct request initiator.
  if (!request.request_initiator.has_value() ||
      !request.request_initiator.value().IsSameOriginWith(request_initiator_)) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] Okta SSO request "
           "skipped because the renderer |request.request_initiator| was empty "
           "or invalid.";
    RecordCorsViolationMetric();
    return false;
  }

  // Only intercept same-origin requests.
  if (!request_initiator_.IsSameOriginWith(request.url)) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] Okta SSO request "
           "skipped because of origin mismatch. The initiator was: "
        << request_initiator_.Serialize() << " vs. "
        << request.url.GetWithEmptyPath().spec();
    RecordCorsViolationMetric();
    return false;
  }

  // Only supported mode is CORS.
  if (request.mode != network::mojom::RequestMode::kCors) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] Okta SSO request skipped because mode != CORS.";
    RecordCorsViolationMetric();
    return false;
  }

  for (const auto& header : request.headers.GetHeaderVector()) {
    const std::string lower_name = base::ToLowerASCII(header.key);
    // Allowing sec-* because they are set in
    // FrameFetchContext::AddClientHintsIfNecessary by blink. Allowing
    // User-Agent because it is not a forbidden header but the function
    // recognises it as such.
    // TODO : b/433226247 - Once https://crrev.com/c/5273743 is merged
    // User-Agent will be accepted by IsSafeHeader.
    if (!base::StartsWith(lower_name, "sec-") && lower_name != "user-agent" &&
        !net::HttpUtil::IsSafeHeader(lower_name, header.value)) {
      LOG_POLICY(ERROR, EXTENSIBLE_SSO)
          << "[OktaEnterpriseSSO] Okta SSO request skipped because "
          << header.key << ": " << header.value << " is a forbidden header.";
      RecordCorsViolationMetric();
      return false;
    }
  }

  return true;
}

ProxyingURLLoaderFactory::~ProxyingURLLoaderFactory() {
  if (destruction_callback_for_testing_) {
    std::move(destruction_callback_for_testing_).Run();
  }
}

bool ProxyingURLLoaderFactory::ScopedURLSessionOverrideForTesting::
    instance_exists_ = false;

ProxyingURLLoaderFactory::ScopedURLSessionOverrideForTesting::
    ScopedURLSessionOverrideForTesting() {
  CHECK_IS_TEST();
  DCHECK(!instance_exists_) << "There should only be one instance of "
                               "ScopedURLSessionOverrideForTesting";
  instance_exists_ = true;
  content::GetNetworkService()->BindTestInterfaceForTesting(  // IN-TEST
      network_service_test_.BindNewPipeAndPassReceiver());
  network_service_test_->SetUseMockURLSessionURLLoaderForTesting(  // IN-TEST
      true);
}

ProxyingURLLoaderFactory::ScopedURLSessionOverrideForTesting::
    ~ScopedURLSessionOverrideForTesting() {
  instance_exists_ = false;
  network_service_test_->SetUseMockURLSessionURLLoaderForTesting(  // IN-TEST
      false);
}

}  // namespace enterprise_auth
