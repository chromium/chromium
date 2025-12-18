// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include <string>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "chrome/browser/enterprise/platform_auth/url_session_url_loader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace enterprise_auth {

namespace {

constexpr std::string_view kPrefix =
    "/idp/idx/authenticators/sso_extension/transactions/";
constexpr std::string_view kSuffix = "/verify";
constexpr size_t kMinPathLength = kPrefix.length() + kSuffix.length() + 1;

}  // namespace

ProxyingURLLoaderFactory::ProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory) {
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
                  kDocumentSubResource) {
    auto [loader_receiver, target_factory] = factory_builder.Append();
    new ProxyingURLLoaderFactory(std::move(loader_receiver),
                                 std::move(target_factory));
  }
}

void ProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (IsOktaSSORequest(request)) {
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

  // Matching the path against pattern: prefix<ID>suffix
  std::string_view path = gurl.path();

  // Normalise the path to not end with '/'.
  if (path.ends_with("/")) {
    path = path.substr(0, path.size() - 1);
  }

  // Not long enough to fit the pattern.
  if (path.length() < kMinPathLength) {
    return false;
  }

  // Matching the prefix and the suffix.
  if (!base::EndsWith(path, kSuffix) || !base::StartsWith(path, kPrefix)) {
    return false;
  }

  // Check that the part between the prefix and the suffix is a single segment.
  size_t id_len = path.length() - kPrefix.length() - kSuffix.length();
  const std::string_view id_part = path.substr(kPrefix.length(), id_len);
  if (id_part.find('/') != std::string_view::npos) {
    return false;
  }

  // Reject URLs with query parameters, fragments, or user credentials.
  if (gurl.has_query() || gurl.has_ref() || gurl.has_username() ||
      gurl.has_password()) {
    return false;
  }

  return true;
}

}  // namespace enterprise_auth
