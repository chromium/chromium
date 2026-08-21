// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_UTILS_H_
#define CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_UTILS_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

namespace content {
class BrowserContext;
}

namespace enterprise_custom_headers {

// Wraps `header_client` with `HttpHeaderInjectionURLLoaderHeaderClient` if
// the HTTP header injection feature is enabled and the policy is available for
// the profile. This allows intercepting network requests to inject enterprise
// headers.
void MaybeWrapTrustedURLLoaderHeaderClient(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client);

// Creates a TrustedHeaderClient that injects enterprise HTTP headers into
// WebSocket connections if the feature is enabled and policy rules exist.
void MaybeCreateWebSocketHeaderClient(
    content::BrowserContext* browser_context,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>* header_client);

}  // namespace enterprise_custom_headers

#endif  // CHROME_BROWSER_ENTERPRISE_NETWORK_HEADER_INJECTION_HTTP_HEADER_INJECTION_UTILS_H_
