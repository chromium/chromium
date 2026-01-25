// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/platform_auth/extensible_enterprise_sso_policy_handler.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_features.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/enterprise/platform_auth/url_session_url_loader.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace enterprise_auth {

ProxyingURLLoaderFactory::ProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    base::flat_set<std::string> configured_hosts)
    : configured_hosts_(std::move(configured_hosts)) {
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
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (enterprise_auth::PlatformAuthProviderManager::GetInstance().IsEnabled() &&
      request_initiator.scheme() == url::kHttpsScheme &&
      type == ChromeContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource &&
      g_browser_process->local_state()
          ->GetList(prefs::kExtensibleEnterpriseSSOEnabledIdps)
          .contains(kOktaIdentityProvider) &&
      g_browser_process->local_state()
          ->GetList(prefs::kExtensibleEnterpriseSSOConfiguredHosts)
          .contains(request_initiator.host())) {
    auto [loader_receiver, target_factory] = factory_builder.Append();

    // Cache configured hosts for a quicker lookup later on.
    const base::ListValue& configured_hosts_pref =
        g_browser_process->local_state()->GetList(
            prefs::kExtensibleEnterpriseSSOConfiguredHosts);
    base::flat_set<std::string> configured_hosts;
    configured_hosts.reserve(configured_hosts_pref.size());
    for (const base::Value& host : configured_hosts_pref) {
      configured_hosts.insert(host.GetString());
    }
    new ProxyingURLLoaderFactory(std::move(loader_receiver),
                                 std::move(target_factory),
                                 std::move(configured_hosts));
  }
}

void ProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (configured_hosts_.contains(request.url.host()) &&
      IsOktaSSORequest(request)) {
    if (intercepted_request_callback_for_testing_) {
      std::move(intercepted_request_callback_for_testing_).Run(request);
    } else {
      URLSessionURLLoader::CreateAndStart(request, std::move(loader_receiver),
                                          std::move(client));
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

ProxyingURLLoaderFactory::~ProxyingURLLoaderFactory() {
  if (destruction_callback_for_testing_) {
    std::move(destruction_callback_for_testing_).Run();
  }
}

// static
bool ProxyingURLLoaderFactory::IsOktaSSORequest(
    const network::ResourceRequest& request) {
  // Only match POST requests.
  if (request.method != "POST") {
    return false;
  }

  const GURL& gurl = request.url;
  // Only match HTTPS requests.
  if (!gurl.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  // Reject URLs with query parameters, fragments, or user credentials.
  if (gurl.has_query() || gurl.has_ref() || gurl.has_username() ||
      gurl.has_password()) {
    return false;
  }

  // Match the URL against the OktaSsoURLPattern parameter.
  static const base::NoDestructor<ContentSettingsPattern> pattern(
      ContentSettingsPattern::FromString(kOktaSsoURLPattern.Get()));
  static bool log_emitted = false;
  if (!pattern->IsValid() && !log_emitted) {
    LOG_POLICY(ERROR, EXTENSIBLE_SSO)
        << "[OktaEnterpriseSSO] invalid OktaSsoURLPattern parameter: "
        << kOktaSsoURLPattern.Get();
    log_emitted = true;
    return false;
  }
  return pattern->Matches(gurl);
}

}  // namespace enterprise_auth
