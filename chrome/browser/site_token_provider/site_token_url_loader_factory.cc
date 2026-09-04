// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/span.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace site_token_provider {

namespace {

constexpr char kMimeTypePlainText[] = "text/plain";

network::mojom::URLResponseHeadPtr BuildResponseHead(
    const url::Origin& initiator) {
  auto head = network::mojom::URLResponseHead::New();
  head->mime_type = kMimeTypePlainText;
  head->headers =
      net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
          .AddHeader(net::HttpRequestHeaders::kContentType, kMimeTypePlainText)
          .AddHeader(network::cors::header_names::kAccessControlAllowOrigin,
                     initiator.Serialize())
          .AddHeader(
              network::cors::header_names::kAccessControlAllowCredentials,
              "true")
          .Build();
  return head;
}

base::expected<mojo::ScopedDataPipeConsumerHandle, net::Error>
CreateDataPipeWithPayload(std::string_view payload) {
  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle) !=
      MOJO_RESULT_OK) {
    return base::unexpected(net::ERR_FAILED);
  }
  if (producer_handle->WriteAllData(base::as_byte_span(payload)) !=
      MOJO_RESULT_OK) {
    return base::unexpected(net::ERR_FAILED);
  }
  return consumer_handle;
}

base::expected<url::Origin, net::Error> ValidateAndGetInitiator(
    const network::ResourceRequest& request,
    int render_process_id) {
  // Verify HTTP method.
  if (!request.method.empty() &&
      request.method != net::HttpRequestHeaders::kGetMethod) {
    return base::unexpected(net::ERR_METHOD_NOT_SUPPORTED);
  }

  // Verify URL scheme, host, and path/query.
  if (!request.url.SchemeIs(
          chrome::kChromeExperimentalSiteTokenProviderScheme) ||
      request.url.host() != chrome::kChromeExperimentalSiteTokenHost ||
      (!request.url.path().empty() && request.url.path() != "/") ||
      request.url.has_query() || request.url.has_ref()) {
    return base::unexpected(net::ERR_INVALID_URL);
  }

  // Verify initiator presence and validity.
  if (!request.request_initiator.has_value() ||
      request.request_initiator->opaque()) {
    return base::unexpected(net::ERR_ACCESS_DENIED);
  }
  const url::Origin& initiator = *request.request_initiator;

  // Ensure the renderer process is authorized for the claimed initiator origin.
  if (!content::ChildProcessSecurityPolicy::GetInstance()
           ->CanAccessDataForOrigin(render_process_id, initiator)) {
    return base::unexpected(net::ERR_ACCESS_DENIED);
  }

  // Require initiator origin to be potentially trustworthy (e.g. HTTPS or
  // localhost).
  if (!network::IsOriginPotentiallyTrustworthy(initiator)) {
    return base::unexpected(net::ERR_ACCESS_DENIED);
  }

  return initiator;
}

base::expected<std::string, net::Error> GetToken(int render_process_id,
                                                 const url::Origin& initiator) {
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(render_process_id);
  Profile* profile =
      rph ? Profile::FromBrowserContext(rph->GetBrowserContext()) : nullptr;
  SiteTokenProviderService* service =
      profile ? SiteTokenProviderServiceFactory::GetForProfile(profile)
              : nullptr;

  if (!service) {
    return base::unexpected(net::ERR_FAILED);
  }

  if (!service->IsDomainAllowlisted(initiator.host())) {
    return base::unexpected(net::ERR_ACCESS_DENIED);
  }

  return service->GetTokenForDomain(initiator.host());
}

}  // namespace

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
SiteTokenURLLoaderFactory::Create(int render_process_id) {
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SiteTokenURLLoaderFactory>(render_process_id),
      pending_remote.InitWithNewPipeAndPassReceiver());
  return pending_remote;
}

SiteTokenURLLoaderFactory::SiteTokenURLLoaderFactory(int render_process_id)
    : render_process_id_(render_process_id) {}

SiteTokenURLLoaderFactory::~SiteTokenURLLoaderFactory() = default;

void SiteTokenURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  mojo::Remote<network::mojom::URLLoaderClient> client(
      std::move(client_remote));

  // Helper lambda to fire the completion on error.
  auto handle_error = [&](net::Error error) {
    client->OnComplete(network::URLLoaderCompletionStatus(error));
  };

  ASSIGN_OR_RETURN(url::Origin initiator,
                   ValidateAndGetInitiator(request, render_process_id_),
                   handle_error);
  ASSIGN_OR_RETURN(std::string token, GetToken(render_process_id_, initiator),
                   handle_error);
  ASSIGN_OR_RETURN(mojo::ScopedDataPipeConsumerHandle consumer_handle,
                   CreateDataPipeWithPayload(token), handle_error);

  client->OnReceiveResponse(BuildResponseHead(initiator),
                            std::move(consumer_handle),
                            /*cached_metadata=*/std::nullopt);
  client->OnComplete(network::URLLoaderCompletionStatus(net::OK));
}

void SiteTokenURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<SiteTokenURLLoaderFactory>(render_process_id_),
      std::move(receiver));
}

}  // namespace site_token_provider
