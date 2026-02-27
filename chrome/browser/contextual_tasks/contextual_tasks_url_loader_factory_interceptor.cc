// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_factory_interceptor.h"

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "components/contextual_tasks/public/features.h"
#include "components/embedder_support/user_agent_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/cors.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

namespace contextual_tasks {

namespace {

const char* const kAuthTokenAllowList[] = {
    "google.com",
    "googlers.com",  // For local servers.
};

const char kAuthorizationHeader[] = "Authorization";
const char kBearerPrefix[] = "Bearer ";
const char kOneGoogleIdentifier[] = "onegoogle";

bool ShouldAddAuthHeader(const GURL& url) {
  // Don't add the Authorization header to OGB URLs, since they don't support
  // OAuth. Attaching an OAuth token can result in 403 errors.
  if (url.spec().contains(kOneGoogleIdentifier)) {
    return false;
  }

  // Only add the Authorization header to domains in the allow list.
  for (const char* domain : kAuthTokenAllowList) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

// Used to determine if a CORS error is due to a preflight request and retry
// the request without the Authorization header if so.
bool IsPreflightError(network::mojom::CorsError error) {
  switch (error) {
    case network::mojom::CorsError::kPreflightInvalidStatus:
    case network::mojom::CorsError::kPreflightDisallowedRedirect:
    case network::mojom::CorsError::kPreflightWildcardOriginNotAllowed:
    case network::mojom::CorsError::kPreflightMissingAllowOriginHeader:
    case network::mojom::CorsError::kPreflightMultipleAllowOriginValues:
    case network::mojom::CorsError::kPreflightInvalidAllowOriginValue:
    case network::mojom::CorsError::kPreflightAllowOriginMismatch:
    case network::mojom::CorsError::kPreflightInvalidAllowCredentials:
    case network::mojom::CorsError::kInvalidAllowMethodsPreflightResponse:
    case network::mojom::CorsError::kInvalidAllowHeadersPreflightResponse:
    case network::mojom::CorsError::kMethodDisallowedByPreflightResponse:
    case network::mojom::CorsError::kHeaderDisallowedByPreflightResponse:
      return true;
    default:
      return false;
  }
}

// A URLLoader that intercepts the response and retries the request without
// the Authorization header if the preflight request fails. This is necessary
// because adding the Authorization header transforms the request from a simple
// request to a preflighted request, which some servers aren't expecting. If the
// preflight request fails, it's likely because the server isn't expecting it,
// so retrying without the Authorization header should succeed.
class ContextualTasksURLLoader : public network::mojom::URLLoader,
                                 public network::mojom::URLLoaderClient {
 public:
  ContextualTasksURLLoader(
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory)
      : client_(std::move(client)),
        request_id_(request_id),
        options_(options),
        request_(request),
        traffic_annotation_(traffic_annotation) {
    target_factory_.Bind(std::move(target_factory));
    StartRequest();
  }

  ContextualTasksURLLoader(const ContextualTasksURLLoader&) = delete;
  ContextualTasksURLLoader& operator=(const ContextualTasksURLLoader&) = delete;

  ~ContextualTasksURLLoader() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    if (target_loader_) {
      target_loader_->FollowRedirect(removed_headers, modified_headers,
                                     modified_cors_exempt_headers, new_url);
    }
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    if (target_loader_) {
      target_loader_->SetPriority(priority, intra_priority_value);
    }
  }

  // network::mojom::URLLoaderClient:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    client_->OnReceiveEarlyHints(std::move(early_hints));
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    client_->OnReceiveResponse(std::move(head), std::move(body),
                               std::move(cached_metadata));
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    client_->OnReceiveRedirect(redirect_info, std::move(head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {
    client_->OnUploadProgress(current_position, total_size,
                              std::move(callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (should_retry_ && status.cors_error_status &&
        IsPreflightError(status.cors_error_status->cors_error)) {
      should_retry_ = false;
      // Retry without auth header.
      request_.headers.RemoveHeader(kAuthorizationHeader);

      // Reset target loader and client receiver.
      target_loader_.reset();
      client_receiver_.reset();

      StartRequest();
      return;
    }
    client_->OnComplete(status);
  }

 private:
  void StartRequest() {
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
    client_receiver_.Bind(client_remote.InitWithNewPipeAndPassReceiver());

    target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, std::move(client_remote), traffic_annotation_);
  }

  mojo::Remote<network::mojom::URLLoaderClient> client_;
  int32_t request_id_;
  uint32_t options_;
  network::ResourceRequest request_;
  net::MutableNetworkTrafficAnnotationTag traffic_annotation_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;

  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
  mojo::Remote<network::mojom::URLLoader> target_loader_;

  bool should_retry_ = true;
};

class ContextualTasksProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  ContextualTasksProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      ContextualTasksUiService* ui_service,
      base::WeakPtr<content::WebContents> web_contents)
      : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver)),
        ui_service_(ui_service ? ui_service->GetWeakPtr() : nullptr),
        web_contents_(web_contents) {
    target_factory_.Bind(std::move(target_factory));
    target_factory_.set_disconnect_handler(base::BindOnce(
        &ContextualTasksProxyingURLLoaderFactory::OnTargetFactoryDisconnected,
        base::Unretained(this)));

    if (base::FeatureList::IsEnabled(
            kContextualTasksSendFullVersionListEnabled)) {
      blink::UserAgentMetadata ua_metadata =
          embedder_support::GetUserAgentMetadata();
      ch_ua_full_version_list_ = ua_metadata.SerializeBrandFullVersionList();
    }
  }

  ~ContextualTasksProxyingURLLoaderFactory() override = default;

  // network::mojom::URLLoaderFactory:
  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    network::ResourceRequest modified_request = request;
    if (base::FeatureList::IsEnabled(
            kContextualTasksSendFullVersionListEnabled) &&
        !ch_ua_full_version_list_.empty()) {
      modified_request.headers.SetHeader("Sec-CH-UA-Full-Version-List",
                                         ch_ua_full_version_list_);
    }

    if (!ui_service_) {
      target_factory_->CreateLoaderAndStart(
          std::move(loader), request_id, options, modified_request,
          std::move(client), traffic_annotation);
      return;
    }

    // Only intercept HTTP/HTTPS requests.
    if (!modified_request.url.SchemeIs(url::kHttpsScheme)) {
      target_factory_->CreateLoaderAndStart(
          std::move(loader), request_id, options, modified_request,
          std::move(client), traffic_annotation);
      return;
    }

    // If the request doesn't need the Authorization header, create the loader
    // and start immediately.
    if (!ShouldAddAuthHeader(modified_request.url)) {
      target_factory_->CreateLoaderAndStart(
          std::move(loader), request_id, options, modified_request,
          std::move(client), traffic_annotation);
      return;
    }

    ui_service_->GetAccessToken(
        base::BindOnce(
            &ContextualTasksProxyingURLLoaderFactory::OnAccessTokenReceived,
            weak_factory_.GetWeakPtr(), std::move(loader), request_id, options,
            modified_request, std::move(client), traffic_annotation),
        web_contents_);
  }

 private:
  void OnTargetFactoryDisconnected() { DisconnectReceiversAndDestroy(); }

  void OnAccessTokenReceived(
      mojo::PendingReceiver<network::mojom::URLLoader> loader,
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      net::MutableNetworkTrafficAnnotationTag traffic_annotation,
      const std::string& token) {
    if (!token.empty()) {
      request.headers.SetHeader(kAuthorizationHeader, kBearerPrefix + token);
    }

    // Clone the target factory to pass to the custom URLLoader.
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_clone;
    target_factory_->Clone(
        target_factory_clone.InitWithNewPipeAndPassReceiver());

    // Create and start a new custom URLLoader which will handle the request.
    // Mainly, the loader will retry the request without the Authorization
    // header if the preflight request fails.
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<ContextualTasksURLLoader>(
            request_id, options, request, std::move(client), traffic_annotation,
            std::move(target_factory_clone)),
        std::move(loader));
  }

  std::string ch_ua_full_version_list_;

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  base::WeakPtr<ContextualTasksUiService> ui_service_;
  base::WeakPtr<content::WebContents> web_contents_;
  base::WeakPtrFactory<ContextualTasksProxyingURLLoaderFactory> weak_factory_{
      this};
};

}  // namespace

void MaybeInterceptURLLoaderFactory(
    content::RenderFrameHost* frame,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!frame) {
    return;
  }

  // Check if this is a WebView guest.
  extensions::WebViewGuest* guest =
      extensions::WebViewGuest::FromRenderFrameHost(frame);
  if (!guest) {
    return;
  }

  // Check if the owner is the Contextual Tasks WebUI.
  content::WebContents* owner_web_contents = guest->owner_web_contents();
  if (!owner_web_contents) {
    return;
  }

  const GURL& owner_url = owner_web_contents->GetLastCommittedURL();
  if (owner_url.scheme() != content::kChromeUIScheme ||
      owner_url.host() != chrome::kChromeUIContextualTasksHost) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(frame->GetProcess()->GetBrowserContext());
  ContextualTasksUiService* ui_service =
      ContextualTasksUiServiceFactory::GetForBrowserContext(profile);

  if (!ui_service) {
    return;
  }

  // Insert the proxy factory.
  auto [receiver, remote] = factory_builder.Append();

  // The proxy factory manages its own lifetime.
  new ContextualTasksProxyingURLLoaderFactory(
      std::move(receiver), std::move(remote), ui_service,
      owner_web_contents->GetWeakPtr());
}

}  // namespace contextual_tasks
