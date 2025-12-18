// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/origin.h"

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
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (enterprise_auth::PlatformAuthProviderManager::GetInstance().IsEnabled()) {
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
  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        options, request, std::move(client),
                                        traffic_annotation);
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
  if (destruction_callback_) {
    std::move(destruction_callback_).Run();
  }
}

// static
bool ProxyingURLLoaderFactory::IsOktaSSORequest(
    const network::ResourceRequest& request) {
  if (request.method != "POST") {
    return false;
  }

  const GURL& gurl = request.url;
  if (!gurl.SchemeIs(url::kHttpsScheme)) {
    return false;
  }

  std::string_view path = gurl.path();
  // Normalise to not end with '/'.
  if (path.ends_with("/")) {
    path = path.substr(0, path.size() - 1);
  }

  if (path.length() < kMinPathLength) {
    return false;
  }

  if (!base::EndsWith(path, kSuffix) || !base::StartsWith(path, kPrefix)) {
    return false;
  }

  size_t id_len = path.length() - kPrefix.length() - kSuffix.length();
  const std::string_view id_part = path.substr(kPrefix.length(), id_len);
  if (id_part.find('/') != std::string_view::npos) {
    return false;
  }

  if (gurl.has_query() || gurl.has_ref() || gurl.has_username() ||
      gurl.has_password()) {
    return false;
  }

  return true;
}

}  // namespace enterprise_auth
