// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_url_loader_throttle.h"

#include <string>
#include <utility>

#include "base/byte_size.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/guest_source.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/public/glic_request_headers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace glic {

namespace {

constexpr char kJavaScriptMimeType[] = "text/javascript";

bool IsGlicHeaderInjectionEnabled(Profile* profile) {
  return base::FeatureList::IsEnabled(features::kGlicHeader) &&
         GlicKeyedService::IsNoWebviewEnabled(profile);
}

bool IsNoWebviewGlicGuestPrimaryMainFrame(content::RenderFrameHost* frame) {
  if (!frame || frame->GetParentOrOuterDocument()) {
    return false;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  if (!web_contents || !IsGlicGuest(web_contents) ||
      !GlicKeyedService::IsNoWebviewEnabled(
          web_contents->GetBrowserContext())) {
    return false;
  }
  content::RenderFrameHost* primary_main_frame =
      web_contents->GetPrimaryMainFrame();
  return primary_main_frame && primary_main_frame->GetFrameTreeNodeId() ==
                                   frame->GetFrameTreeNodeId();
}

// Navigation throttles run before a final RenderFrameHost is selected, so this
// helper identifies the primary main frame via (WebContents*, FrameTreeNodeId)
// rather than RenderFrameHost* (unlike IsNoWebviewGlicGuestPrimaryMainFrame).
bool ShouldInjectGlicHeaders(content::WebContents* web_contents,
                             content::FrameTreeNodeId frame_tree_node_id) {
  Profile* profile =
      web_contents
          ? Profile::FromBrowserContext(web_contents->GetBrowserContext())
          : nullptr;
  if (!IsGlicHeaderInjectionEnabled(profile)) {
    return false;
  }
  if (!web_contents || !IsGlicGuest(web_contents)) {
    return false;
  }
  if (frame_tree_node_id && web_contents->GetPrimaryMainFrame() &&
      web_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId() !=
          frame_tree_node_id) {
    return false;
  }
  return true;
}

void SetOnboardingCompletedHeader(net::HttpRequestHeaders* headers,
                                  Profile* profile) {
  const bool onboarding_completed =
      profile ? GlicEnabling::HasConsentedForProfile(profile) : false;
  headers->SetHeader(kGlicOnboardingCompletedHeaderName,
                     onboarding_completed ? "true" : "false");
}

void MaybeSetOnboardingArmHeader(net::HttpRequestHeaders* headers,
                                 Profile* profile) {
  if (std::optional<std::string> onboarding_arm = GetOnboardingArm(profile);
      onboarding_arm.has_value()) {
    headers->SetHeader(kGlicOnboardingArmHeaderName, *onboarding_arm);
  }
}

// Self-deleting URLLoader that streams the in-memory Glic API bundle over a
// Mojo data pipe and closes the producer pipe before notifying OnComplete.
class GlicBundleURLLoader final : public network::mojom::URLLoader {
 public:
  static void Start(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const GURL& request_url,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
      std::string payload) {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 0;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      mojo::Remote<network::mojom::URLLoaderClient> client(
          std::move(client_remote));
      client->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      return;
    }
    new GlicBundleURLLoader(std::move(loader_receiver), request_url,
                            std::move(client_remote),
                            std::move(producer_handle),
                            std::move(consumer_handle), std::move(payload));
  }

  GlicBundleURLLoader(const GlicBundleURLLoader&) = delete;
  GlicBundleURLLoader& operator=(const GlicBundleURLLoader&) = delete;

  // network::mojom::URLLoader:
  void FollowRedirect(
      network::HttpRequestHeadersUpdateParams headers_update_params,
      const std::optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}

 private:
  GlicBundleURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      const GURL& request_url,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote,
      mojo::ScopedDataPipeProducerHandle producer_handle,
      mojo::ScopedDataPipeConsumerHandle consumer_handle,
      std::string payload)
      : receiver_(this, std::move(loader_receiver)),
        client_(std::move(client_remote)),
        payload_size_(payload.size()) {
    receiver_.set_disconnect_handler(
        base::BindOnce(&GlicBundleURLLoader::Destroy, base::Unretained(this)));
    client_.set_disconnect_handler(
        base::BindOnce(&GlicBundleURLLoader::Destroy, base::Unretained(this)));

    auto head = network::mojom::URLResponseHead::New();
    const base::Time now = base::Time::Now();
    const base::TimeTicks now_ticks = base::TimeTicks::Now();
    head->request_time = now;
    head->response_time = now;
    head->request_start = now_ticks;
    head->response_start = now_ticks;
    head->load_timing.request_start_time = now;
    head->load_timing.request_start = now_ticks;
    head->load_timing.receive_headers_start = now_ticks;
    head->load_timing.receive_headers_end = now_ticks;
    head->mime_type = kJavaScriptMimeType;
    head->charset = "utf-8";
    head->content_length = static_cast<int64_t>(payload_size_);
    head->encoded_data_length = static_cast<int64_t>(payload_size_);
    head->should_use_source_hash_for_js_code_cache = true;
    head->headers = net::HttpResponseHeaders::Builder({1, 1}, "200 OK")
                        .AddHeader(net::HttpRequestHeaders::kContentType,
                                   "text/javascript; charset=utf-8")
                        .AddHeader("X-Content-Type-Options", "nosniff")
                        .Build();
    head->parsed_headers =
        network::PopulateParsedHeaders(head->headers.get(), request_url);

    client_->OnReceiveResponse(std::move(head), std::move(consumer_handle),
                               /*cached_metadata=*/std::nullopt);

    producer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    producer_->Write(
        std::make_unique<mojo::StringDataSource>(
            payload, mojo::StringDataSource::AsyncWritingMode::
                         STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(&GlicBundleURLLoader::OnWriteComplete,
                       base::Unretained(this)));
  }

  ~GlicBundleURLLoader() override = default;

  void OnWriteComplete(MojoResult result) {
    // Close the producer end before notifying OnComplete, as required by
    // network::mojom::URLLoaderClient::OnReceiveResponse.
    producer_.reset();
    if (result != MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      Destroy();
      return;
    }
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = base::ByteSize(payload_size_);
    status.encoded_body_length = base::ByteSize(payload_size_);
    status.decoded_body_length = base::ByteSize(payload_size_);
    status.completion_time = base::TimeTicks::Now();
    client_->OnComplete(status);
    Destroy();
  }

  void Destroy() { delete this; }

  mojo::Receiver<network::mojom::URLLoader> receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> client_;
  const size_t payload_size_;
  std::unique_ptr<mojo::DataPipeProducer> producer_;
};

}  // namespace

// static
std::unique_ptr<GlicURLLoaderThrottle> GlicURLLoaderThrottle::MaybeCreate(
    Profile* profile,
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    content::FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& request) {
  CHECK(profile);
  content::WebContents* web_contents = wc_getter ? wc_getter.Run() : nullptr;
  if (!ShouldInjectGlicHeaders(web_contents, frame_tree_node_id)) {
    return nullptr;
  }

  return std::make_unique<GlicURLLoaderThrottle>(profile->GetWeakPtr());
}

// static
void GlicURLLoaderThrottle::SetHeaders(net::HttpRequestHeaders* headers,
                                       Profile* profile) {
  headers->SetHeader(kGlicHeaderName, kGlicHeaderValue);
  headers->SetHeader(kGlicVersionHeaderName, version_info::GetVersionNumber());
  headers->SetHeader(kGlicChannelHeaderName,
                     version_info::GetChannelString(chrome::GetChannel()));
  if (GetGlicProviderSpec(profile).SupportsSsr()) {
    SetOnboardingCompletedHeader(headers, profile);
    MaybeSetOnboardingArmHeader(headers, profile);
  }
  if (base::FeatureList::IsEnabled(features::kGlicDisableHfcr)) {
    headers->SetHeader(kGlicDisableHfcrHeaderName, "1");
  }
}

GlicURLLoaderThrottle::GlicURLLoaderThrottle() = default;

GlicURLLoaderThrottle::GlicURLLoaderThrottle(base::WeakPtr<Profile> profile)
    : profile_(profile) {}

GlicURLLoaderThrottle::~GlicURLLoaderThrottle() = default;

void GlicURLLoaderThrottle::WillStartRequest(network::ResourceRequest* request,
                                             bool* defer) {
  SetHeaders(&request->cors_exempt_headers, profile_.get());
}

void GlicURLLoaderThrottle::WillRedirectRequest(
    net::RedirectInfo* redirect_info,
    const network::mojom::URLResponseHead& response_head,
    bool* defer,
    network::HttpRequestHeadersUpdateParams* headers_update_params) {
  SetHeaders(&headers_update_params->modified_cors_exempt_headers,
             profile_.get());
}

GlicSubresourceProxyingURLLoaderFactory::
    GlicSubresourceProxyingURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
        mojo::PendingRemote<network::mojom::URLLoaderFactory>
            target_factory_remote,
        base::WeakPtr<Profile> profile,
        content::GlobalRenderFrameHostId frame_id,
        url::Origin initiator,
        bool inject_headers,
        bool serve_api_bundle,
        base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key),
      profile_(profile),
      frame_id_(frame_id),
      initiator_(std::move(initiator)),
      inject_headers_(inject_headers),
      serve_api_bundle_(serve_api_bundle) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(base::BindOnce(
      &GlicSubresourceProxyingURLLoaderFactory::OnTargetFactoryError,
      base::Unretained(this)));
}

GlicSubresourceProxyingURLLoaderFactory::
    ~GlicSubresourceProxyingURLLoaderFactory() = default;

// static
void GlicSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* frame,
    const url::Origin& request_initiator,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!IsNoWebviewGlicGuestPrimaryMainFrame(frame)) {
    return;
  }
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(frame);
  CHECK(web_contents);
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  if (!profile) {
    return;
  }

  const bool inject_headers =
      base::FeatureList::IsEnabled(features::kGlicHeader);
  const bool serve_api_bundle =
      GetGlicProviderSpec(profile).SupportsApiBundleResource() &&
      IsOriginAllowedGlicApi(request_initiator, profile);
  if (!inject_headers && !serve_api_bundle) {
    return;
  }

  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<GlicSubresourceProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote), profile->GetWeakPtr(),
      frame->GetGlobalId(), request_initiator, inject_headers,
      serve_api_bundle);
}

void GlicSubresourceProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  if (IsBundleRequest(request)) {
    HandleBundleRequest(std::move(loader_receiver), request, std::move(client));
    return;
  }

  if (inject_headers_) {
    network::ResourceRequest modified_request = request;
    GlicURLLoaderThrottle::SetHeaders(&modified_request.cors_exempt_headers,
                                      profile_.get());
    target_factory_->CreateLoaderAndStart(
        std::move(loader_receiver), request_id, options, modified_request,
        std::move(client), traffic_annotation);
    return;
  }

  target_factory_->CreateLoaderAndStart(std::move(loader_receiver), request_id,
                                        options, request, std::move(client),
                                        traffic_annotation);
}

bool GlicSubresourceProxyingURLLoaderFactory::IsBundleRequest(
    const network::ResourceRequest& request) const {
  return serve_api_bundle_ && request.url.is_valid() &&
         request.url.SchemeIsHTTPOrHTTPS() &&
         base::EndsWith(request.url.path(), kGlicApiBundleResourcePathSuffix,
                        base::CompareCase::SENSITIVE) &&
         url::Origin::Create(request.url).IsSameOriginWith(initiator_);
}

void GlicSubresourceProxyingURLLoaderFactory::HandleBundleRequest(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  content::RenderFrameHost* rfh = content::RenderFrameHost::FromID(frame_id_);
  // Subresource `CreateLoaderAndStart` can race ahead of `DidCommitNavigation`
  // on a speculative (`kPendingCommit`) or initial empty (`kActive` with an
  // invalid `GetLastCommittedURL()`) RFH. In those states `initiator_` was
  // already validated via `IsOriginAllowedGlicApi` in `MaybeProxyRequest`; once
  // a navigation has committed, also verify the committed frame via
  // `IsFrameAllowedGlicApi`.
  const bool frame_allowed =
      IsNoWebviewGlicGuestPrimaryMainFrame(rfh) &&
      (rfh->GetLifecycleState() ==
           content::RenderFrameHost::LifecycleState::kPendingCommit ||
       (rfh->GetLifecycleState() ==
            content::RenderFrameHost::LifecycleState::kActive &&
        (!rfh->GetLastCommittedURL().is_valid() ||
         IsFrameAllowedGlicApi(*rfh))));
  const bool request_allowed =
      frame_allowed &&
      (request.method.empty() ||
       request.method == net::HttpRequestHeaders::kGetMethod) &&
      request.request_initiator.has_value() &&
      request.request_initiator->IsSameOriginWith(initiator_);
  if (!request_allowed) {
    mojo::Remote<network::mojom::URLLoaderClient> client_remote(
        std::move(client));
    client_remote->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_ACCESS_DENIED));
    return;
  }

  GlicBundleURLLoader::Start(std::move(loader_receiver), request.url,
                             std::move(client), GetGuestAPISource());
}

void GlicSubresourceProxyingURLLoaderFactory::OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace glic
