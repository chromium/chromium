// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/chrome_signin_proxying_url_loader_factory.h"

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/not_fatal_until.h"
#include "base/supports_user_data.h"
#include "base/types/optional_util.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_helper.h"
#include "chrome/browser/signin/header_modification_delegate.h"
#include "chrome/browser/signin/header_modification_delegate_impl.h"
#include "components/signin/core/browser/signin_header_helper.h"
#include "components/signin/public/base/signin_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/buildflags/buildflags.h"
#include "google_apis/gaia/gaia_urls.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/android/tab_web_contents_delegate_android.h"
#endif

namespace signin {

namespace {

// User data key for BrowserContextData.
const void* const kBrowserContextUserDataKey = &kBrowserContextUserDataKey;

// Owns all of the ProxyingURLLoaderFactorys for a given Profile.
class BrowserContextData : public base::SupportsUserData::Data {
 public:
  BrowserContextData(const BrowserContextData&) = delete;
  BrowserContextData& operator=(const BrowserContextData&) = delete;

  ~BrowserContextData() override {}

  static void StartProxying(Profile* profile,
                            const net::IsolationInfo& factory_isolation_info,
                            content::WebContents::Getter web_contents_getter,
                            network::URLLoaderFactoryBuilder& factory_builder) {
    auto* self = static_cast<BrowserContextData*>(
        profile->GetUserData(kBrowserContextUserDataKey));
    if (!self) {
      self = new BrowserContextData();
      profile->SetUserData(kBrowserContextUserDataKey, base::WrapUnique(self));
    }

#if BUILDFLAG(IS_ANDROID)
    bool is_custom_tab = false;
    content::WebContents* web_contents = web_contents_getter.Run();
    if (web_contents) {
      auto* delegate =
          TabAndroid::FromWebContents(web_contents)
              ? static_cast<android::TabWebContentsDelegateAndroid*>(
                    web_contents->GetDelegate())
              : nullptr;
      is_custom_tab = delegate && delegate->IsCustomTab();
    }
    auto delegate = std::make_unique<HeaderModificationDelegateImpl>(
        profile, /*incognito_enabled=*/!is_custom_tab);
#else
    auto delegate = std::make_unique<HeaderModificationDelegateImpl>(profile);
#endif
    auto proxy = std::make_unique<ProxyingURLLoaderFactory>(
        std::move(delegate), factory_isolation_info,
        std::move(web_contents_getter), factory_builder,
        base::BindOnce(&BrowserContextData::RemoveProxy,
                       self->weak_factory_.GetWeakPtr()));
    self->proxies_.emplace(std::move(proxy));
  }

  void RemoveProxy(ProxyingURLLoaderFactory* proxy) {
    auto it = proxies_.find(proxy);
    CHECK(it != proxies_.end(), base::NotFatalUntil::M130);
    proxies_.erase(it);
  }

 private:
  BrowserContextData() {}

  std::set<std::unique_ptr<ProxyingURLLoaderFactory>, base::UniquePtrComparator>
      proxies_;

  base::WeakPtrFactory<BrowserContextData> weak_factory_{this};
};

}  // namespace

class ProxyingURLLoaderFactory::InProgressRequest
    : public network::mojom::URLLoader,
      public network::mojom::URLLoaderClient,
      public base::SupportsUserData {
 public:
  InProgressRequest(
      ProxyingURLLoaderFactory* factory,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& request,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation);

  InProgressRequest(const InProgressRequest&) = delete;
  InProgressRequest& operator=(const InProgressRequest&) = delete;

  ~InProgressRequest() override {
    if (destruction_callback_)
      std::move(destruction_callback_).Run();
  }

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;

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
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    target_client_->OnReceiveEarlyHints(std::move(early_hints));
  }
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override {
    target_client_->OnUploadProgress(current_position, total_size,
                                     std::move(callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    network::RecordOnTransferSizeUpdatedUMA(
        network::OnTransferSizeUpdatedFrom::kProxyingURLLoaderFactory);

    target_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    target_client_->OnComplete(status);
  }

  const url::Origin* GetTopFrameOrigin() const;

 private:
  class ProxyRequestAdapter;
  class ProxyResponseAdapter;

  void OnBindingsClosed() {
    // Destroys |this|.
    factory_->RemoveRequest(this);
  }

  // Back pointer to the factory which owns this class.
  const raw_ptr<ProxyingURLLoaderFactory> factory_;

  // Information about the current request.
  GURL request_url_;
  GURL response_url_;
  // Refers to the "last" referrer in the redirect chain.
  GURL referrer_;
  // The origin that initiated the request. May be empty for browser-initiated
  // requests. See network::ResourceRequest::request_initiator for details.
  std::optional<url::Origin> request_initiator_;
  // Top frame origin of the request, if specified. May be empty for
  // renderer-initiated requests, as those requests do not set "trusted"
  // parameters. In that case, the top frame origin will likely be specified at
  // the URLLoaderFactory level.
  std::optional<url::Origin> request_top_frame_origin_;
  net::HttpRequestHeaders headers_;
  net::HttpRequestHeaders cors_exempt_headers_;
  net::RedirectInfo redirect_info_;
  const network::mojom::RequestDestination request_destination_;
  const bool is_outermost_main_frame_;
  const bool is_fetch_like_api_;

  base::OnceClosure destruction_callback_;

  // Messages received by |client_receiver_| are forwarded to |target_client_|.
  mojo::Receiver<network::mojom::URLLoaderClient> client_receiver_{this};
  mojo::Remote<network::mojom::URLLoaderClient> target_client_;

  // Messages received by |loader_receiver_| are forwarded to |target_loader_|.
  mojo::Receiver<network::mojom::URLLoader> loader_receiver_;
  mojo::Remote<network::mojom::URLLoader> target_loader_;
};

class ProxyingURLLoaderFactory::InProgressRequest::ProxyRequestAdapter
    : public ChromeRequestAdapter {
 public:
  // Does not take |modified_cors_exempt_headers| just because we don't have a
  // use-case to modify it in this class now.
  ProxyRequestAdapter(InProgressRequest* in_progress_request,
                      const net::HttpRequestHeaders& original_headers,
                      net::HttpRequestHeaders* modified_headers,
                      std::vector<std::string>* removed_headers)
      : ChromeRequestAdapter(in_progress_request->request_url_,
                             original_headers,
                             modified_headers,
                             removed_headers),
        in_progress_request_(in_progress_request) {
    DCHECK(in_progress_request_);
  }

  ProxyRequestAdapter(const ProxyRequestAdapter&) = delete;
  ProxyRequestAdapter& operator=(const ProxyRequestAdapter&) = delete;

  ~ProxyRequestAdapter() override = default;

  content::WebContents::Getter GetWebContentsGetter() const override {
    return in_progress_request_->factory_->web_contents_getter_;
  }

  network::mojom::RequestDestination GetRequestDestination() const override {
    return in_progress_request_->request_destination_;
  }

  bool IsOutermostMainFrame() const override {
    return in_progress_request_->is_outermost_main_frame_;
  }

  bool IsFetchLikeAPI() const override {
    return in_progress_request_->is_fetch_like_api_;
  }

  GURL GetReferrer() const override { return in_progress_request_->referrer_; }

  void SetDestructionCallback(base::OnceClosure closure) override {
    if (!in_progress_request_->destruction_callback_)
      in_progress_request_->destruction_callback_ = std::move(closure);
  }

 private:
  const raw_ptr<InProgressRequest> in_progress_request_;
};

class ProxyingURLLoaderFactory::InProgressRequest::ProxyResponseAdapter
    : public ResponseAdapter {
 public:
  ProxyResponseAdapter(InProgressRequest* in_progress_request,
                       net::HttpResponseHeaders* headers)
      : in_progress_request_(in_progress_request), headers_(headers) {
    DCHECK(in_progress_request_);
    DCHECK(headers_);
  }

  ProxyResponseAdapter(const ProxyResponseAdapter&) = delete;
  ProxyResponseAdapter& operator=(const ProxyResponseAdapter&) = delete;

  ~ProxyResponseAdapter() override = default;

  // signin::ResponseAdapter
  content::WebContents::Getter GetWebContentsGetter() const override {
    return in_progress_request_->factory_->web_contents_getter_;
  }

  bool IsOutermostMainFrame() const override {
    return in_progress_request_->is_outermost_main_frame_;
  }

  GURL GetUrl() const override { return in_progress_request_->response_url_; }

  std::optional<url::Origin> GetRequestInitiator() const override {
    return in_progress_request_->request_initiator_;
  }

  const url::Origin* GetRequestTopFrameOrigin() const override {
    return in_progress_request_->GetTopFrameOrigin();
  }

  const net::HttpResponseHeaders* GetHeaders() const override {
    return headers_;
  }

  void RemoveHeader(const std::string& name) override {
    headers_->RemoveHeader(name);
  }

  base::SupportsUserData::Data* GetUserData(const void* key) const override {
    return in_progress_request_->GetUserData(key);
  }

  void SetUserData(
      const void* key,
      std::unique_ptr<base::SupportsUserData::Data> data) override {
    in_progress_request_->SetUserData(key, std::move(data));
  }

 private:
  const raw_ptr<InProgressRequest, DanglingUntriaged> in_progress_request_;
  const raw_ptr<net::HttpResponseHeaders, DanglingUntriaged> headers_;
};

ProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    ProxyingURLLoaderFactory* factory,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
    : factory_(factory),
      request_url_(request.url),
      response_url_(request.url),
      referrer_(request.referrer),
      request_initiator_(request.request_initiator),
      request_destination_(request.destination),
      is_outermost_main_frame_(request.is_outermost_main_frame),
      is_fetch_like_api_(request.is_fetch_like_api),
      target_client_(std::move(client)),
      loader_receiver_(this, std::move(loader_receiver)) {
  if (request.trusted_params) {
    request_top_frame_origin_ =
        request.trusted_params->isolation_info.top_frame_origin();
  }

  mojo::PendingRemote<network::mojom::URLLoaderClient> proxy_client =
      client_receiver_.BindNewPipeAndPassRemote();

  net::HttpRequestHeaders modified_headers;
  std::vector<std::string> removed_headers;
  ProxyRequestAdapter adapter(this, request.headers, &modified_headers,
                              &removed_headers);
  factory_->delegate_->ProcessRequest(&adapter, GURL() /* redirect_url */);

  if (modified_headers.IsEmpty() && removed_headers.empty()) {
    factory_->target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), request_id, options,
        request, std::move(proxy_client), traffic_annotation);

    // We need to keep a full copy of the request headers in case there is a
    // redirect and the request headers need to be modified again.
    headers_ = request.headers;
    cors_exempt_headers_ = request.cors_exempt_headers;
  } else {
    network::ResourceRequest request_copy = request;
    request_copy.headers.MergeFrom(modified_headers);
    for (const std::string& name : removed_headers) {
      request_copy.headers.RemoveHeader(name);
      request_copy.cors_exempt_headers.RemoveHeader(name);
    }

    factory_->target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), request_id, options,
        request_copy, std::move(proxy_client), traffic_annotation);

    headers_.Swap(&request_copy.headers);
    cors_exempt_headers_.Swap(&request_copy.cors_exempt_headers);
  }

  base::RepeatingClosure closure = base::BarrierClosure(
      2, base::BindOnce(&InProgressRequest::OnBindingsClosed,
                        base::Unretained(this)));
  loader_receiver_.set_disconnect_handler(closure);
  client_receiver_.set_disconnect_handler(closure);
}

void ProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers_ext,
    const net::HttpRequestHeaders& modified_headers_ext,
    const net::HttpRequestHeaders& modified_cors_exempt_headers_ext,
    const std::optional<GURL>& opt_new_url) {
  std::vector<std::string> removed_headers = removed_headers_ext;
  net::HttpRequestHeaders modified_headers = modified_headers_ext;
  net::HttpRequestHeaders modified_cors_exempt_headers =
      modified_cors_exempt_headers_ext;
  ProxyRequestAdapter adapter(this, headers_, &modified_headers,
                              &removed_headers);
  factory_->delegate_->ProcessRequest(&adapter, redirect_info_.new_url);

  headers_.MergeFrom(modified_headers);
  cors_exempt_headers_.MergeFrom(modified_cors_exempt_headers);
  for (const std::string& name : removed_headers) {
    headers_.RemoveHeader(name);
    cors_exempt_headers_.RemoveHeader(name);
  }

  target_loader_->FollowRedirect(removed_headers, modified_headers,
                                 modified_cors_exempt_headers, opt_new_url);

  request_url_ = redirect_info_.new_url;
  referrer_ = GURL(redirect_info_.new_referrer);
}

void ProxyingURLLoaderFactory::InProgressRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  // Even though |head| is const we can get a non-const pointer to the headers
  // and modifications we made are passed to the target client.
  ProxyResponseAdapter adapter(this, head->headers.get());
  factory_->delegate_->ProcessResponse(&adapter, GURL() /* redirect_url */);
  target_client_->OnReceiveResponse(std::move(head), std::move(body),
                                    std::move(cached_metadata));
}

void ProxyingURLLoaderFactory::InProgressRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // Even though |head| is const we can get a non-const pointer to the headers
  // and modifications we made are passed to the target client.
  ProxyResponseAdapter adapter(this, head->headers.get());
  factory_->delegate_->ProcessResponse(&adapter, redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, std::move(head));

  // The request URL returned by ProxyResponseAdapter::GetURL() is updated
  // immediately but the URL and referrer
  redirect_info_ = redirect_info;
  response_url_ = redirect_info.new_url;
}

const url::Origin*
ProxyingURLLoaderFactory::InProgressRequest::GetTopFrameOrigin() const {
  if (factory_->top_frame_origin_.has_value()) {
    return &factory_->top_frame_origin_.value();
  }

  return base::OptionalToPtr(request_top_frame_origin_);
}

ProxyingURLLoaderFactory::ProxyingURLLoaderFactory(
    std::unique_ptr<HeaderModificationDelegate> delegate,
    const net::IsolationInfo& factory_isolation_info,
    content::WebContents::Getter web_contents_getter,
    network::URLLoaderFactoryBuilder& factory_builder,
    DisconnectCallback on_disconnect) {
  DCHECK(proxy_receivers_.empty());
  DCHECK(!delegate_);
  DCHECK(!web_contents_getter_);
  DCHECK(!on_disconnect_);

  auto [loader_receiver, target_factory] = factory_builder.Append();
  DCHECK(!target_factory_.is_bound());

  delegate_ = std::move(delegate);
  top_frame_origin_ = factory_isolation_info.top_frame_origin();
  web_contents_getter_ = std::move(web_contents_getter);
  on_disconnect_ = std::move(on_disconnect);

  target_factory_.Bind(std::move(target_factory));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &ProxyingURLLoaderFactory::OnTargetFactoryError, base::Unretained(this)));

  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &ProxyingURLLoaderFactory::OnProxyBindingError, base::Unretained(this)));
}

ProxyingURLLoaderFactory::~ProxyingURLLoaderFactory() = default;

// static
void ProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* render_frame_host,
    bool is_navigation,
    const url::Origin& request_initiator,
    const net::IsolationInfo& factory_isolation_info,
    network::URLLoaderFactoryBuilder& factory_builder) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Navigation requests are handled using signin::URLLoaderThrottle.
  if (is_navigation)
    return;

  if (!render_frame_host)
    return;

  // This proxy should only be installed for subresource requests from a frame
  // that is rendering the GAIA signon realm.
  if (request_initiator != GaiaUrls::GetInstance()->gaia_origin())
    return;

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  auto* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (profile->IsOffTheRecord()) {
#if BUILDFLAG(ENABLE_BOUND_SESSION_CREDENTIALS)
    if (!switches::IsBoundSessionCredentialsEnabled(profile->GetPrefs())) {
      return;
    }
#else
    return;
#endif
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // Most requests from guest web views are ignored.
  if (HeaderModificationDelegateImpl::ShouldIgnoreGuestWebViewRequest(
          web_contents)) {
    return;
  }
#endif

  auto web_contents_getter =
      base::BindRepeating(&content::WebContents::FromFrameTreeNodeId,
                          render_frame_host->GetFrameTreeNodeId());

  BrowserContextData::StartProxying(profile, factory_isolation_info,
                                    std::move(web_contents_getter),
                                    factory_builder);
}

void ProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  requests_.insert(std::make_unique<InProgressRequest>(
      this, std::move(loader_receiver), request_id, options, request,
      std::move(client), traffic_annotation));
}

void ProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void ProxyingURLLoaderFactory::OnTargetFactoryError() {
  // Stop calls to CreateLoaderAndStart() when |target_factory_| is invalid.
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeDestroySelf();
}

void ProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_receivers_.empty())
    target_factory_.reset();

  MaybeDestroySelf();
}

void ProxyingURLLoaderFactory::RemoveRequest(InProgressRequest* request) {
  auto it = requests_.find(request);
  CHECK(it != requests_.end(), base::NotFatalUntil::M130);
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
