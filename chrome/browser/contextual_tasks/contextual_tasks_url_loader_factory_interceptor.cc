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
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/backoff_entry.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace contextual_tasks {

namespace {

const char* const kAuthTokenAllowList[] = {
    "google.com",
    "googlers.com",  // For local servers.
};

bool ShouldAddAuthHeader(const GURL& url) {
  // Only add the Authorization header to domains in the allow list.
  for (const char* domain : kAuthTokenAllowList) {
    if (url.DomainIs(domain)) {
      return true;
    }
  }
  return false;
}

class ContextualTasksProxyingURLLoaderFactory
    : public network::SelfDeletingURLLoaderFactory {
 public:
  ContextualTasksProxyingURLLoaderFactory(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      ContextualTasksUiService* ui_service)
      : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver)),
        ui_service_(ui_service ? ui_service->GetWeakPtr() : nullptr) {
    target_factory_.Bind(std::move(target_factory));
    target_factory_.set_disconnect_handler(base::BindOnce(
        &ContextualTasksProxyingURLLoaderFactory::OnTargetFactoryDisconnected,
        base::Unretained(this)));
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
    if (!ui_service_) {
      target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                            options, request, std::move(client),
                                            traffic_annotation);
      return;
    }

    // Only intercept HTTP/HTTPS requests.
    if (!request.url.SchemeIs(url::kHttpsScheme)) {
      target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                            options, request, std::move(client),
                                            traffic_annotation);
      return;
    }

    // If the request doesn't need the Authorization header, create the loader
    // and start immediately.
    if (!ShouldAddAuthHeader(request.url)) {
      target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                            options, request, std::move(client),
                                            traffic_annotation);
      return;
    }

    ui_service_->GetAccessToken(base::BindOnce(
        &ContextualTasksProxyingURLLoaderFactory::OnAccessTokenReceived,
        weak_factory_.GetWeakPtr(), std::move(loader), request_id, options,
        request, std::move(client), traffic_annotation));
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
      request.headers.SetHeader("Authorization", "Bearer " + token);
    }
    target_factory_->CreateLoaderAndStart(std::move(loader), request_id,
                                          options, request, std::move(client),
                                          traffic_annotation);
  }

  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  base::WeakPtr<ContextualTasksUiService> ui_service_;
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
  new ContextualTasksProxyingURLLoaderFactory(std::move(receiver),
                                              std::move(remote), ui_service);
}

}  // namespace contextual_tasks
