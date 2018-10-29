// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_proxying_url_loader_factory.h"

#include <utility>

#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/renderer_host/auto_login_parser.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_utils.h"
#include "net/http/http_util.h"

namespace android_webview {

namespace {

const char kAutoLoginHeaderName[] = "X-Auto-Login";

// Handles intercepted, in-progress requests/responses, so that they can be
// controlled and modified accordingly.
class InterceptedRequest : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient {
 public:
  InterceptedRequest(
      int process_id,
      uint64_t request_id,
      int32_t routing_id,
      uint32_t options,
      const network::ResourceRequest& request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client,
      network::mojom::URLLoaderFactoryPtr target_factory);
  ~InterceptedRequest() override;

  void Restart();

  // network::mojom::URLLoaderClient
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader
  void FollowRedirect(const base::Optional<std::vector<std::string>>&
                          to_be_removed_request_headers,
                      const base::Optional<net::HttpRequestHeaders>&
                          modified_request_headers) override;
  void ProceedWithResponse() override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

 private:
  void OnRequestError(const network::URLLoaderCompletionStatus& status);

  // TODO(timvolodine): consider factoring this out of this class.
  void OnReceivedErrorToCallback(int error_code);

  const int process_id_;
  const uint64_t request_id_;
  const int32_t routing_id_;
  const uint32_t options_;

  network::ResourceRequest request_;
  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  mojo::Binding<network::mojom::URLLoader> proxied_loader_binding_;
  network::mojom::URLLoaderClientPtr target_client_;

  mojo::Binding<network::mojom::URLLoaderClient> proxied_client_binding_;
  network::mojom::URLLoaderPtr target_loader_;
  network::mojom::URLLoaderFactoryPtr target_factory_;

  base::WeakPtrFactory<InterceptedRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterceptedRequest);
};

InterceptedRequest::InterceptedRequest(
    int process_id,
    uint64_t request_id,
    int32_t routing_id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client,
    network::mojom::URLLoaderFactoryPtr target_factory)
    : process_id_(process_id),
      request_id_(request_id),
      routing_id_(routing_id),
      options_(options),
      request_(request),
      traffic_annotation_(traffic_annotation),
      proxied_loader_binding_(this, std::move(loader_request)),
      target_client_(std::move(client)),
      proxied_client_binding_(this),
      target_factory_(std::move(target_factory)),
      weak_factory_(this) {
  // If there is a client error, clean up the request.
  target_client_.set_connection_error_handler(base::BindOnce(
      &InterceptedRequest::OnRequestError, weak_factory_.GetWeakPtr(),
      network::URLLoaderCompletionStatus(net::ERR_ABORTED)));
}

InterceptedRequest::~InterceptedRequest() {}

void InterceptedRequest::Restart() {
  // TODO(timvolodine): add async check shouldOverrideUrlLoading and
  // shouldInterceptRequest.

  if (!target_loader_ && target_factory_) {
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), routing_id_, request_id_, options_,
        request_, std::move(proxied_client), traffic_annotation_);
  }
}

namespace {
// TODO(timvolodine): consider factoring this out of this file.

AwContentsClientBridge* GetAwContentsClientBridgeFromID(int process_id,
                                                        int render_frame_id) {
  content::WebContents* wc =
      process_id
          ? content::WebContents::FromRenderFrameHost(
                content::RenderFrameHost::FromID(process_id, render_frame_id))
          : content::WebContents::FromFrameTreeNodeId(render_frame_id);
  return AwContentsClientBridge::FromWebContents(wc);
}

void OnReceivedHttpErrorOnUiThread(
    int process_id,
    int render_frame_id,
    const AwWebResourceRequest& request,
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> http_error_info) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, std::move(http_error_info));
}

void OnReceivedErrorOnUiThread(int process_id,
                               int render_frame_id,
                               const AwWebResourceRequest& request,
                               int error_code) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedError dropped for "
                  << request.url;
    return;
  }
  // TODO(timvolodine): properly handle safe_browsing_hit.
  client->OnReceivedError(request, error_code, false /*safebrowsing_hit*/);
}

void OnNewLoginRequestOnUiThread(int process_id,
                                 int render_frame_id,
                                 const std::string& realm,
                                 const std::string& account,
                                 const std::string& args) {
  auto* client = GetAwContentsClientBridgeFromID(process_id, render_frame_id);
  if (!client) {
    return;
  }
  client->NewLoginRequest(realm, account, args);
}

}  // namespace

// URLLoaderClient methods.

void InterceptedRequest::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  // intercept response headers here
  // pause/resume proxied_client_binding_ if necessary

  if (head.headers->response_code() >= 400) {
    // In Android WebView the WebViewClient.onReceivedHttpError callback
    // is invoked for any resource (main page, iframe, image, etc.) with
    // status code >= 400.
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> error_info =
        AwContentsClientBridge::ExtractHttpErrorInfo(head.headers.get());

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(
            &OnReceivedHttpErrorOnUiThread, process_id_,
            request_.render_frame_id,
            AwWebResourceRequest(
                request_.url.spec(), request_.method,
                request_.resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
                request_.has_user_gesture, request_.headers),
            std::move(error_info)));
  }

  if (request_.resource_type == content::RESOURCE_TYPE_MAIN_FRAME) {
    // Check for x-auto-login-header
    HeaderData header_data;
    std::string header_string;
    if (head.headers->GetNormalizedHeader(kAutoLoginHeaderName,
                                          &header_string)) {
      if (ParseHeader(header_string, ALLOW_ANY_REALM, &header_data)) {
        // TODO(timvolodine): consider simplifying this and above callback
        // code, crbug.com/897149.
        base::PostTaskWithTraits(
            FROM_HERE, {content::BrowserThread::UI},
            base::BindOnce(&OnNewLoginRequestOnUiThread, process_id_,
                           request_.render_frame_id, header_data.realm,
                           header_data.account, header_data.args));
      }
    }
  }

  target_client_->OnReceiveResponse(head);
}

void InterceptedRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  // TODO(timvolodine): handle redirect override.
  // TODO(timvolodine): handle unsafe redirect case.
  target_client_->OnReceiveRedirect(redirect_info, head);
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
}

void InterceptedRequest::OnUploadProgress(int64_t current_position,
                                          int64_t total_size,
                                          OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void InterceptedRequest::OnReceiveCachedMetadata(
    const std::vector<uint8_t>& data) {
  target_client_->OnReceiveCachedMetadata(data);
}

void InterceptedRequest::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void InterceptedRequest::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  target_client_->OnStartLoadingResponseBody(std::move(body));
}

void InterceptedRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    OnRequestError(status);
    return;
  }

  target_client_->OnComplete(status);

  // Deletes |this|.
  // TODO(timvolodine): consider doing this via the factory.
  delete this;
}

// URLLoader methods.

void InterceptedRequest::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  if (target_loader_) {
    target_loader_->FollowRedirect(to_be_removed_request_headers,
                                   modified_request_headers);
  }

  Restart();
}

void InterceptedRequest::ProceedWithResponse() {
  if (target_loader_)
    target_loader_->ProceedWithResponse();
}

void InterceptedRequest::SetPriority(net::RequestPriority priority,
                                     int32_t intra_priority_value) {
  if (target_loader_)
    target_loader_->SetPriority(priority, intra_priority_value);
}

void InterceptedRequest::PauseReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->PauseReadingBodyFromNet();
}

void InterceptedRequest::ResumeReadingBodyFromNet() {
  if (target_loader_)
    target_loader_->ResumeReadingBodyFromNet();
}

void InterceptedRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status) {
  target_client_->OnComplete(status);

  OnReceivedErrorToCallback(status.error_code);

  // Deletes |this|.
  // TODO(timvolodine): consider handling this through a data structure.
  delete this;
}

void InterceptedRequest::OnReceivedErrorToCallback(int error_code) {
  // TODO(timvolodine): add constructor for direct creation from
  // network::ResourceRequest.
  AwWebResourceRequest aw_request(
      request_.url.spec(), request_.method,
      request_.resource_type == content::RESOURCE_TYPE_MAIN_FRAME,
      request_.has_user_gesture, request_.headers);
  // Error callback now requires for |is_renderer_intiated| to be set.
  aw_request.is_renderer_initiated = ui::PageTransitionIsWebTriggerable(
      static_cast<ui::PageTransition>(request_.transition_type));

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           base::BindOnce(&OnReceivedErrorOnUiThread,
                                          process_id_, request_.render_frame_id,
                                          std::move(aw_request), error_code));
}

}  // namespace

//============================
// AwProxyingURLLoaderFactory
//============================

AwProxyingURLLoaderFactory::AwProxyingURLLoaderFactory(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler)
    : process_id_(process_id),
      request_handler_(std::move(request_handler)),
      weak_factory_(this) {
  // actual creation of the factory
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  target_factory_.Bind(std::move(target_factory_info));
  target_factory_.set_connection_error_handler(
      base::BindOnce(&AwProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(
      base::BindRepeating(&AwProxyingURLLoaderFactory::OnProxyBindingError,
                          base::Unretained(this)));
}

AwProxyingURLLoaderFactory::~AwProxyingURLLoaderFactory() {}

// static
void AwProxyingURLLoaderFactory::CreateProxy(
    int process_id,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    std::unique_ptr<AwInterceptedRequestHandler> request_handler) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // will manage its own lifetime
  new AwProxyingURLLoaderFactory(process_id, std::move(loader_request),
                                 std::move(target_factory_info),
                                 std::move(request_handler));
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  bool pass_through = false;
  if (pass_through) {
    // this is the so-called pass-through, no-op option.
    target_factory_->CreateLoaderAndStart(
        std::move(loader), routing_id, request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  // TODO(timvolodine): handle interception, modification (headers for
  // webview), blocking, callbacks etc..

  network::mojom::URLLoaderFactoryPtr target_factory_clone;
  target_factory_->Clone(MakeRequest(&target_factory_clone));

  // manages its own lifecycle
  // TODO(timvolodine): consider keeping track of requests.
  InterceptedRequest* req = new InterceptedRequest(
      process_id_, request_id, routing_id, options, request, traffic_annotation,
      std::move(loader), std::move(client), std::move(target_factory_clone));
  req->Restart();
}

void AwProxyingURLLoaderFactory::OnTargetFactoryError() {
  delete this;
}

void AwProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_bindings_.empty())
    delete this;
}

void AwProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

}  // namespace android_webview
