// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_utils.h"

#include <utility>

#include "chrome/browser/enterprise/network_header_injection/http_header_injection_client.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_service_factory.h"
#include "chrome/browser/enterprise/network_header_injection/http_header_injection_url_loader_header_client.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/network_header_injection/core/features.h"
#include "components/enterprise/network_header_injection/core/http_header_injection_service.h"

namespace enterprise_custom_headers {

void MaybeWrapTrustedURLLoaderHeaderClient(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client) {
  if (!header_client || !IsHttpHeaderInjectionEnabled()) {
    return;
  }

  auto* service = HttpHeaderInjectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!service || !service->HasRules()) {
    return;
  }

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      original_header_client;
  if (*header_client) {
    original_header_client = std::move(*header_client);
  }

  mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>
      new_header_client;
  HttpHeaderInjectionURLLoaderHeaderClient::Create(
      service->GetWeakPtr(), new_header_client.InitWithNewPipeAndPassReceiver(),
      std::move(original_header_client));

  *header_client = std::move(new_header_client);
}

void MaybeCreateWebSocketHeaderClient(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>* header_client) {
  if (!header_client || !IsHttpHeaderInjectionEnabled()) {
    return;
  }

  auto* service = HttpHeaderInjectionServiceFactory::GetForProfile(
      Profile::FromBrowserContext(browser_context));
  if (!service || !service->HasRules()) {
    return;
  }

  mojo::PendingRemote<network::mojom::TrustedHeaderClient> new_client;
  HttpHeaderInjectionClient::Create(service->GetWeakPtr(),
                                    new_client.InitWithNewPipeAndPassReceiver(),
                                    mojo::NullRemote());
  *header_client = std::move(new_client);
}

}  // namespace enterprise_custom_headers
