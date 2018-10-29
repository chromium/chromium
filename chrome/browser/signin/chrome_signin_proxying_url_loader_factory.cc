// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"

#include "base/barrier_closure.h"
#include "base/task/post_task.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/net_errors.h"

namespace signin {

namespace {

// User data key for ResourceContextData.
const void* const kResourceContextUserDataKey = &kResourceContextUserDataKey;

// Owns all of the ProxyingURLLoaderFactorys for a given Profile. Since these
// live on the IO thread this is done indirectly through the
// content::ResourceContext.
class ResourceContextData : public base::SupportsUserData::Data {
 public:
  ~ResourceContextData() override {}

  static void StartProxying(
      content::ResourceContext* resource_context,
      content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
      network::mojom::URLLoaderFactoryRequest request,
      network::mojom::URLLoaderFactoryPtrInfo target_factory) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    auto* self = static_cast<ResourceContextData*>(
        resource_context->GetUserData(kResourceContextUserDataKey));
    if (!self) {
      self = new ResourceContextData();
      resource_context->SetUserData(kResourceContextUserDataKey,
                                    base::WrapUnique(self));
    }

    auto delegate =
        std::make_unique<HeaderModificationDelegateImpl>(resource_context);
    auto proxy = std::make_unique<ProxyingURLLoaderFactory>(
        std::move(delegate), std::move(web_contents_getter), std::move(request),
        std::move(target_factory),
        base::BindOnce(&ResourceContextData::RemoveProxy,
                       self->weak_factory_.GetWeakPtr()));
    self->proxies_.emplace(std::move(proxy));
  }

  void RemoveProxy(ProxyingURLLoaderFactory* proxy) {
    auto it = proxies_.find(proxy);
    DCHECK(it != proxies_.end());
    proxies_.erase(it);
  }

 private:
  ResourceContextData() : weak_factory_(this) {}

  std::set<std::unique_ptr<ProxyingURLLoaderFactory>, base::UniquePtrComparator>
      proxies_;

  base::WeakPtrFactory<ResourceContextData> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ResourceContextData);
};

}  // namespace

class ProxyingURLLoaderFactory::InProgressRequest
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient {
 public:
  InProgressRequest(
      ProxyingURLLoaderFactory* factory,
      network::mojom::URLLoaderRequest loader_request,
      int32_t routing_id,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      network::mojom::URLLoaderClientPtr client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  ~InProgressRequest() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  // network::mojom::URLLoader:
  void FollowRedirect(
      const base::Optional<std::vector<std::string>>& to_be_removed_headers,
      const base::Optional<net::HttpRequestHeaders>& modified_request_headers)
      override;

  void ProceedWithResponse() override { target_loader_->ProceedWithResponse(); }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    target_loader_->SetPriority(priority, intra_priority_value);
  }

  void PauseReadingBodyFromNet() override {
    target_loader_->PauseReadingBodyFromNet();
  }

  void ResumeReadingBodyFromNet() override {
    target_loader_->ResumeReadingBodyFromNet();
  }

  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(const network::ResourceResponseHead& head) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         const network::ResourceResponseHead& head) override;

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    target_client_->OnUploadProgress(current_position, total_size,
                                     std::move(callback));
  }

  void OnReceiveCachedMetadata(const std::vector<uint8_t>& data) override {
    target_client_->OnReceiveCachedMetadata(data);
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    target_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnStartLoadingResponseBody(
      mojo::ScopedDataPipeConsumerHandle body) override {
    target_client_->OnStartLoadingResponseBody(std::move(body));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    target_client_->OnComplete(status);
  }

 private:
  class ProxyRequestAdapter;
  class ProxyResponseAdapter;

  void OnBindingsClosed() {
    // Destroys |this|.
    factory_->RemoveRequest(this);
  }

  // Back pointer to the factory which owns this class.
  ProxyingURLLoaderFactory* const factory_;

  // Information about the current request.
  GURL request_url_;
  GURL response_url_;
  GURL referrer_origin_;
  net::HttpRequestHeaders headers_;
  net::RedirectInfo redirect_info_;
  const content::ResourceType resource_type_;
  const bool is_main_frame_;

  base::OnceClosure destruction_callback_;

  // Messages received by |client_binding_| are forwarded to |target_client_|.
  mojo::Binding<network::mojom::URLLoaderClient> client_binding_;
  network::mojom::URLLoaderClientPtr target_client_;

  // Messages received by |loader_binding_| are forwarded to |target_loader_|.
  mojo::Binding<network::mojom::URLLoader> loader_binding_;
  network::mojom::URLLoaderPtr target_loader_;

  DISALLOW_COPY_AND_ASSIGN(InProgressRequest);
};

class ProxyingURLLoaderFactory::InProgressRequest::ProxyRequestAdapter
    : public ChromeRequestAdapter {
 public:
  ProxyRequestAdapter(InProgressRequest* in_progress_request,
                      const net::HttpRequestHeaders& original_headers,
                      net::HttpRequestHeaders* modified_headers,
                      std::vector<std::string>* headers_to_remove)
      : ChromeRequestAdapter(nullptr),
        in_progress_request_(in_progress_request),
        original_headers_(original_headers),
        modified_headers_(modified_headers),
        headers_to_remove_(headers_to_remove) {
    DCHECK(in_progress_request_);
    DCHECK(modified_headers_);
    DCHECK(headers_to_remove_);
  }

  ~ProxyRequestAdapter() override = default;

  // signin::ChromeRequestAdapter
  bool IsMainRequestContext(ProfileIOData* io_data) override {
    // This code is never reached from other request contexts.
    return true;
  }

  content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter()
      const override {
    return in_progress_request_->factory_->web_contents_getter_;
  }

  content::ResourceType GetResourceType() const override {
    return in_progress_request_->resource_type_;
  }

  GURL GetReferrerOrigin() const override {
    return in_progress_request_->referrer_origin_;
  }

  void SetDestructionCallback(base::OnceClosure closure) override {
    if (!in_progress_request_->destruction_callback_)
      in_progress_request_->destruction_callback_ = std::move(closure);
  }

  // signin::RequestAdapter
  const GURL& GetUrl() override { return in_progress_request_->request_url_; }

  bool HasHeader(const std::string& name) override {
    return (original_headers_.HasHeader(name) ||
            modified_headers_->HasHeader(name)) &&
           !base::ContainsValue(*headers_to_remove_, name);
  }

  void RemoveRequestHeaderByName(const std::string& name) override {
    if (!base::ContainsValue(*headers_to_remove_, name))
      headers_to_remove_->push_back(name);
  }

  void SetExtraHeaderByName(const std::string& name,
                            const std::string& value) override {
    modified_headers_->SetHeader(name, value);

    auto it =
        std::find(headers_to_remove_->begin(), headers_to_remove_->end(), name);
    if (it != headers_to_remove_->end())
      headers_to_remove_->erase(it);
  }

 private:
  InProgressRequest* const in_progress_request_;
  const net::HttpRequestHeaders& original_headers_;
  net::HttpRequestHeaders* const modified_headers_;
  std::vector<std::string>* const headers_to_remove_;

  DISALLOW_COPY_AND_ASSIGN(ProxyRequestAdapter);
};

class ProxyingURLLoaderFactory::InProgressRequest::ProxyResponseAdapter
    : public ResponseAdapter {
 public:
  ProxyResponseAdapter(const InProgressRequest* in_progress_request,
                       net::HttpResponseHeaders* headers)
      : ResponseAdapter(nullptr),
        in_progress_request_(in_progress_request),
        headers_(headers) {
    DCHECK(in_progress_request_);
    DCHECK(headers_);
  }

  ~ProxyResponseAdapter() override = default;

  // signin::ResponseAdapter
  content::ResourceRequestInfo::WebContentsGetter GetWebContentsGetter()
      const override {
    return in_progress_request_->factory_->web_contents_getter_;
  }

  bool IsMainFrame() const override {
    return in_progress_request_->is_main_frame_;
  }

  GURL GetOrigin() const override {
    return in_progress_request_->response_url_.GetOrigin();
  }

  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_;
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

 private:
  const InProgressRequest* const in_progress_request_;
  net::HttpResponseHeaders* const headers_;

  DISALLOW_COPY_AND_ASSIGN(ProxyResponseAdapter);
};

ProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    ProxyingURLLoaderFactory* factory,
    network::mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : factory_(factory),
      request_url_(request.url),
      response_url_(request.url),
      referrer_origin_(request.referrer.GetOrigin()),
      resource_type_(static_cast<content::ResourceType>(request.resource_type)),
      is_main_frame_(request.is_main_frame),
      client_binding_(this),
      target_client_(std::move(client)),
      loader_binding_(this, std::move(loader_request)) {
  network::mojom::URLLoaderClientPtr proxy_client;
  client_binding_.Bind(mojo::MakeRequest(&proxy_client));

  net::HttpRequestHeaders modified_headers;
  std::vector<std::string> headers_to_remove;
  ProxyRequestAdapter adapter(this, request.headers, &modified_headers,
                              &headers_to_remove);
  factory_->delegate_->ProcessRequest(&adapter, GURL() /* redirect_url */);

  if (modified_headers.IsEmpty() && headers_to_remove.empty()) {
    factory_->target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), routing_id, request_id, options,
        request, std::move(proxy_client), traffic_annotation);

    // We need to keep a full copy of the request headers in case there is a
    // redirect and the request headers need to be modified again.
    headers_.CopyFrom(request.headers);
  } else {
    network::ResourceRequest request_copy = request;
    request_copy.headers.MergeFrom(modified_headers);
    for (const std::string& name : headers_to_remove)
      request_copy.headers.RemoveHeader(name);

    factory_->target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), routing_id, request_id, options,
        request_copy, std::move(proxy_client), traffic_annotation);

    headers_.Swap(&request_copy.headers);
  }

  base::RepeatingClosure closure = base::BarrierClosure(
      2, base::BindOnce(&InProgressRequest::OnBindingsClosed,
                        base::Unretained(this)));
  loader_binding_.set_connection_error_handler(closure);
  client_binding_.set_connection_error_handler(closure);
}

void ProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const base::Optional<std::vector<std::string>>& opt_headers_to_remove,
    const base::Optional<net::HttpRequestHeaders>& opt_modified_headers) {
  std::vector<std::string> headers_to_remove;
  if (opt_headers_to_remove)
    headers_to_remove = *opt_headers_to_remove;

  net::HttpRequestHeaders modified_headers;
  if (opt_modified_headers)
    modified_headers.CopyFrom(*opt_modified_headers);

  ProxyRequestAdapter adapter(this, headers_, &modified_headers,
                              &headers_to_remove);
  factory_->delegate_->ProcessRequest(&adapter, redirect_info_.new_url);

  headers_.MergeFrom(modified_headers);
  for (const std::string& name : headers_to_remove)
    headers_.RemoveHeader(name);

  target_loader_->FollowRedirect(
      headers_to_remove.empty() ? base::nullopt
                                : base::make_optional(headers_to_remove),
      modified_headers.IsEmpty() ? base::nullopt
                                 : base::make_optional(modified_headers));

  request_url_ = redirect_info_.new_url;
  referrer_origin_ = GURL(redirect_info_.new_referrer).GetOrigin();
}

void ProxyingURLLoaderFactory::InProgressRequest::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  // Even though |head| is const we can get a non-const pointer to the headers
  // and modifications we made are passed to the target client.
  ProxyResponseAdapter adapter(this, head.headers.get());
  factory_->delegate_->ProcessResponse(&adapter, GURL() /* redirect_url */);
  target_client_->OnReceiveResponse(head);
}

void ProxyingURLLoaderFactory::InProgressRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  // Even though |head| is const we can get a non-const pointer to the headers
  // and modifications we made are passed to the target client.
  ProxyResponseAdapter adapter(this, head.headers.get());
  factory_->delegate_->ProcessResponse(&adapter, redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, head);

  // The request URL returned by ProxyResponseAdapter::GetOrigin() is updated
  // immediately but the URL and referrer
  redirect_info_ = redirect_info;
  response_url_ = redirect_info.new_url;
}

ProxyingURLLoaderFactory::ProxyingURLLoaderFactory(
    std::unique_ptr<HeaderModificationDelegate> delegate,
    content::ResourceRequestInfo::WebContentsGetter web_contents_getter,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory,
    DisconnectCallback on_disconnect) {
  DCHECK(proxy_bindings_.empty());
  DCHECK(!target_factory_.is_bound());
  DCHECK(!delegate_);
  DCHECK(!web_contents_getter_);
  DCHECK(!on_disconnect_);

  delegate_ = std::move(delegate);
  web_contents_getter_ = std::move(web_contents_getter);
  on_disconnect_ = std::move(on_disconnect);

  target_factory_.Bind(std::move(target_factory));
  target_factory_.set_connection_error_handler(base::BindOnce(
      &ProxyingURLLoaderFactory::OnTargetFactoryError, base::Unretained(this)));

  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(base::BindRepeating(
      &ProxyingURLLoaderFactory::OnProxyBindingError, base::Unretained(this)));
}

ProxyingURLLoaderFactory::~ProxyingURLLoaderFactory() = default;

// static
bool ProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* render_frame_host,
    bool is_navigation,
    const url::Origin& request_initiator,
    network::mojom::URLLoaderFactoryRequest* factory_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Navigation requests are handled using signin::URLLoaderThrottle.
  if (is_navigation)
    return false;

  // This proxy should only be installed for subresource requests from a frame
  // that is rendering the GAIA signon realm.
  if (!gaia::IsGaiaSignonRealm(request_initiator.GetURL()))
    return false;

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord())
    return false;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Account consistency requires the AccountReconcilor, which is only
  // attached to the main request context.
  // Note: InlineLoginUI uses an isolated request context and thus bypasses
  // the account consistency flow here. See http://crbug.com/428396
  if (extensions::WebViewRendererState::GetInstance()->IsGuest(
          render_frame_host->GetProcess()->GetID())) {
    return false;
  }
#endif

  auto proxied_request = std::move(*factory_request);
  network::mojom::URLLoaderFactoryPtrInfo target_factory_info;
  *factory_request = mojo::MakeRequest(&target_factory_info);

  auto web_contents_getter =
      base::BindRepeating(&content::WebContents::FromFrameTreeNodeId,
                          render_frame_host->GetFrameTreeNodeId());

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&ResourceContextData::StartProxying,
                     profile->GetResourceContext(),
                     std::move(web_contents_getter), std::move(proxied_request),
                     std::move(target_factory_info)));
  return true;
}

void ProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  requests_.insert(std::make_unique<InProgressRequest>(
      this, std::move(loader_request), routing_id, request_id, options, request,
      std::move(client), traffic_annotation));
}

void ProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

void ProxyingURLLoaderFactory::OnTargetFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |target_factory_| is invalid.
  target_factory_.reset();
  proxy_bindings_.CloseAllBindings();

  MaybeDestroySelf();
}

void ProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_bindings_.empty())
    target_factory_.reset();

  MaybeDestroySelf();
}

void ProxyingURLLoaderFactory::RemoveRequest(InProgressRequest* request) {
  auto it = requests_.find(request);
  DCHECK(it != requests_.end());
  requests_.erase(it);

  MaybeDestroySelf();
}

void ProxyingURLLoaderFactory::MaybeDestroySelf() {
  // Even if all URLLoaderFactory pipes connected to this object have been
  // closed it has to stay alive until all active requests have completed.
  if (target_factory_.is_bound() || !requests_.empty())
    return;

  // Deletes |this|.
  std::move(on_disconnect_).Run(this);
}

}  // namespace signin
