// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_cic_proxy_loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/byte_size.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/data_pipe_producer.h"
#include "mojo/public/cpp/system/string_data_source.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/url_util.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/url_constants.h"

namespace glic {

namespace {

constexpr char kProdCicBaseUrl[] = "https://glic-cic.apps.corp.google.com/";
constexpr char kCicPathSegment[] = "/cic/";
constexpr char kGlicPassthroughQueryParam[] = "glic_passthrough";
constexpr int kMaxRedirects = 10;

constexpr char kSyntheticCsp[] =
    "default-src 'none'; base-uri 'none'; form-action 'none'; "
    "frame-ancestors 'none'";

constexpr std::array<std::string_view, 10> kAllowedResponseHeaders = {
    "Content-Type",
    "Content-Length",
    "Content-Encoding",
    "Cache-Control",
    "ETag",
    "Last-Modified",
    "Vary",
    "X-Content-Type-Options",
    "Accept-Ranges",
    "Content-Range",
};

std::string BuildCorpSsoExpiredJson(const GURL& cic_base_url) {
  base::DictValue payload;
  payload.Set("error", "CORP_SSO_EXPIRED");
  payload.Set("loginUrl", cic_base_url.spec());
  return base::WriteJson(payload).value_or(
      R"json({"error":"CORP_SSO_EXPIRED"})json");
}

std::string BuildCorpSsoRecoveryHtml(const GURL& cic_base_url) {
  return base::StrCat(
      {"<!doctype html><title>Corp SSO Session Expired</title>"
       "<p>Corp SSO session expired. <a href=\"",
       base::EscapeForHTML(cic_base_url.spec()),
       "\" target=\"_blank\" rel=\"noopener noreferrer\">Sign in to CiC</a>, "
       "then <a href=\"\">Reload</a>.</p>"});
}

scoped_refptr<net::HttpResponseHeaders> RebuildAllowedResponseHeaders(
    const net::HttpResponseHeaders& source) {
  auto rebuilt = net::HttpResponseHeaders::TryToCreate(source.GetStatusLine());
  if (!rebuilt) {
    rebuilt = net::HttpResponseHeaders::Builder(
                  net::HttpVersion(1, 1),
                  base::NumberToString(source.response_code()))
                  .Build();
  }
  for (std::string_view header_name : kAllowedResponseHeaders) {
    size_t iter = 0;
    while (std::optional<std::string_view> value =
               source.EnumerateHeader(&iter, header_name)) {
      if (net::HttpUtil::IsValidHeaderValue(*value)) {
        rebuilt->AddHeader(header_name, *value);
      }
    }
  }
  return rebuilt;
}

bool IsInNoWebviewGlicGuestPrimaryPage(content::RenderFrameHost* frame,
                                       bool* is_primary_main_frame) {
  if (!frame || frame->IsNestedWithinFencedFrame()) {
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
  content::RenderFrameHost* main_frame = frame->GetMainFrame();
  if (!primary_main_frame || !main_frame ||
      main_frame->GetFrameTreeNodeId() !=
          primary_main_frame->GetFrameTreeNodeId()) {
    return false;
  }
  *is_primary_main_frame =
      !frame->GetParentOrOuterDocument() &&
      frame->GetFrameTreeNodeId() == primary_main_frame->GetFrameTreeNodeId();
  return true;
}

GURL BuildTargetUrlWithPath(const GURL& cic_base_url,
                            const GURL& request_url,
                            std::string_view target_path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(target_path);
  if (request_url.has_query()) {
    replacements.SetQueryStr(request_url.query());
  } else {
    replacements.ClearQuery();
  }
  replacements.ClearRef();
  return cic_base_url.ReplaceComponents(replacements);
}

bool IsCorpLoginRedirect(const GURL& url) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  return url.DomainIs("login.corp.google.com") ||
         url.DomainIs("accounts.googlers.com");
}

bool IsSafeProxyPath(std::string_view path) {
  if (path.find('\\') != std::string_view::npos ||
      path.find('\0') != std::string_view::npos) {
    return false;
  }
  const std::string lower_path = base::ToLowerASCII(path);
  if (lower_path.find("%2e") != std::string::npos ||
      lower_path.find("%2f") != std::string::npos ||
      lower_path.find("%5c") != std::string::npos ||
      lower_path.find("%00") != std::string::npos) {
    return false;
  }
  for (std::string_view segment : base::SplitStringPiece(
           path, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY)) {
    if (segment == "." || segment == "..") {
      return false;
    }
  }
  return true;
}

bool IsGlicDocumentPath(std::string_view path, std::string_view guest_path) {
  if (!guest_path.empty() && path == guest_path) {
    return true;
  }
  return base::EndsWith(path, "/glic", base::CompareCase::SENSITIVE) ||
         base::EndsWith(path, "/glic/", base::CompareCase::SENSITIVE);
}

bool HasAllowedRequestBody(const network::ResourceRequestBody* request_body) {
  if (!request_body || !request_body->elements()) {
    return true;
  }
  for (const network::DataElement& element : *request_body->elements()) {
    switch (element.type()) {
      case network::DataElement::Tag::kBytes:
      case network::DataElement::Tag::kDataPipe:
      case network::DataElement::Tag::kChunkedDataPipe:
        break;
      case network::DataElement::Tag::kFile:
        return false;
    }
  }
  return true;
}

void CopyAllowlistedHeaders(const net::HttpRequestHeaders& source,
                            net::HttpRequestHeaders* dest) {
  const std::array<std::string_view, 3> kAllowedHeaders = {
      net::HttpRequestHeaders::kAccept,
      net::HttpRequestHeaders::kContentType,
      net::HttpRequestHeaders::kIfNoneMatch,
  };
  for (std::string_view header_name : kAllowedHeaders) {
    if (std::optional<std::string> value = source.GetHeader(header_name);
        value.has_value()) {
      dest->SetHeader(header_name, *value);
    }
  }
}

void CompleteClientWithError(
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    int net_error) {
  mojo::Remote<network::mojom::URLLoaderClient> client_remote(
      std::move(client));
  client_remote->OnComplete(network::URLLoaderCompletionStatus(net_error));
}

// Self-deleting URLLoader that proxies a single request from the Glic guest
// origin to `cic_base_url` via the profile's default StoragePartition
// URLLoaderFactory, intercepting corp SSO redirects into a local recovery
// response without leaking the redirect into Blink's guest origin.
class GlicCicProxyURLLoader final : public network::mojom::URLLoader,
                                    public network::mojom::URLLoaderClient {
 public:
  static void Start(
      mojo::PendingReceiver<network::mojom::URLLoader> downstream_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> downstream_client,
      scoped_refptr<network::SharedURLLoaderFactory> upstream_factory,
      const network::ResourceRequest& original_request,
      const std::string& validated_method,
      net::HttpRequestHeaders validated_headers,
      uint32_t upstream_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const GURL& cic_base_url,
      GURL target_url,
      content::ContentBrowserClient::URLLoaderFactoryType type) {
    new GlicCicProxyURLLoader(
        std::move(downstream_receiver), std::move(downstream_client),
        std::move(upstream_factory), original_request, validated_method,
        std::move(validated_headers), upstream_options, traffic_annotation,
        cic_base_url, std::move(target_url), type);
  }

  GlicCicProxyURLLoader(const GlicCicProxyURLLoader&) = delete;
  GlicCicProxyURLLoader& operator=(const GlicCicProxyURLLoader&) = delete;

  // network::mojom::URLLoader:
  void FollowRedirect(
      network::HttpRequestHeadersUpdateParams headers_update_params,
      const std::optional<GURL>& new_url) override {
    // Upstream redirects are handled internally and never surfaced to the
    // downstream caller.
    downstream_receiver_.ReportBadMessage("Unexpected FollowRedirect");
    Destroy();
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    if (upstream_loader_) {
      upstream_loader_->SetPriority(priority, intra_priority_value);
    }
  }

  // network::mojom::URLLoaderClient:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    // Drop upstream early hints until final non-redirect response headers are
    // confirmed.
  }

  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    if (!url::Origin::Create(current_upstream_url_)
             .IsSameOriginWith(cic_origin_)) {
      // Upstream bounced to a corp login host and returned an interactive
      // response instead of silently redirecting back to `cic_origin_`.
      SendCorpSsoRecoveryResponse();
      return;
    }
    if (head && head->headers) {
      head->headers = RebuildAllowedResponseHeaders(*head->headers);
      head->parsed_headers = network::PopulateParsedHeaders(
          head->headers.get(), original_request_url_);
    }
    downstream_client_->OnReceiveResponse(std::move(head), std::move(body),
                                          /*cached_metadata=*/std::nullopt);
  }

  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override {
    const bool is_cic_origin = redirect_info.new_url.is_valid() &&
                               redirect_info.new_url.SchemeIsHTTPOrHTTPS() &&
                               url::Origin::Create(redirect_info.new_url)
                                   .IsSameOriginWith(cic_origin_);
    const bool is_corp_login = IsCorpLoginRedirect(redirect_info.new_url);

    if (!is_cic_origin && !is_corp_login) {
      if (IsCorpLoginRedirect(current_upstream_url_)) {
        // The corp login host redirected to an external interactive IdP.
        SendCorpSsoRecoveryResponse();
        return;
      }
      upstream_loader_.reset();
      upstream_client_receiver_.reset();
      downstream_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
      Destroy();
      return;
    }

    if (redirect_count_ >= kMaxRedirects) {
      if (IsCorpLoginRedirect(current_upstream_url_) || is_corp_login) {
        SendCorpSsoRecoveryResponse();
        return;
      }
      upstream_loader_.reset();
      upstream_client_receiver_.reset();
      downstream_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
      Destroy();
      return;
    }

    ++redirect_count_;
    current_upstream_url_ = redirect_info.new_url;
    upstream_loader_->FollowRedirect(network::HttpRequestHeadersUpdateParams(),
                                     std::nullopt);
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        base::OnceCallback<void()> callback) override {
    downstream_client_->OnUploadProgress(current_position, total_size,
                                         std::move(callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    downstream_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    upstream_loader_.reset();
    upstream_client_receiver_.reset();
    downstream_client_->OnComplete(status);
    Destroy();
  }

 private:
  GlicCicProxyURLLoader(
      mojo::PendingReceiver<network::mojom::URLLoader> downstream_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> downstream_client,
      scoped_refptr<network::SharedURLLoaderFactory> upstream_factory,
      const network::ResourceRequest& original_request,
      const std::string& validated_method,
      net::HttpRequestHeaders validated_headers,
      uint32_t upstream_options,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      const GURL& cic_base_url,
      GURL target_url,
      content::ContentBrowserClient::URLLoaderFactoryType type)
      : downstream_receiver_(this, std::move(downstream_receiver)),
        downstream_client_(std::move(downstream_client)),
        original_request_url_(original_request.url),
        cic_base_url_(cic_base_url),
        cic_origin_(url::Origin::Create(cic_base_url)),
        current_upstream_url_(std::move(target_url)),
        type_(type),
        is_head_request_(validated_method ==
                         net::HttpRequestHeaders::kHeadMethod) {
    downstream_receiver_.set_disconnect_handler(base::BindOnce(
        &GlicCicProxyURLLoader::Destroy, base::Unretained(this)));
    downstream_client_.set_disconnect_handler(base::BindOnce(
        &GlicCicProxyURLLoader::Destroy, base::Unretained(this)));

    network::ResourceRequest upstream_request;
    upstream_request.url = current_upstream_url_;
    upstream_request.method = validated_method;
    if (type_ == content::ContentBrowserClient::URLLoaderFactoryType::
                     kDocumentSubResource) {
      upstream_request.request_body = original_request.request_body;
    }
    upstream_request.headers = std::move(validated_headers);
    upstream_request.priority = original_request.priority;
    upstream_request.priority_incremental =
        original_request.priority_incremental;
    upstream_request.enable_upload_progress =
        original_request.enable_upload_progress;
    upstream_request.enable_load_timing = original_request.enable_load_timing;
    // Forward `throttling_profile_id` so CDP `Network.emulateNetworkConditions`
    // throttling applies across the proxy boundary
    // (`network::ThrottlingController` is a network-service-wide singleton
    // keyed by this token). Leave `devtools_request_id` and `devtools_observer`
    // unset so cross-origin corp SSO redirects, cookies, and raw upstream
    // headers are never exposed to the guest's DevTools observer. Guard against
    // empty `UnguessableToken` values, which Mojo rejects during
    // deserialization.
    if (original_request.throttling_profile_id &&
        !original_request.throttling_profile_id->is_empty()) {
      upstream_request.throttling_profile_id =
          original_request.throttling_profile_id;
    }
    upstream_request.credentials_mode =
        network::mojom::CredentialsMode::kInclude;
    const url::Origin upstream_origin =
        url::Origin::Create(upstream_request.url);
    upstream_request.site_for_cookies =
        net::SiteForCookies::FromOrigin(upstream_origin);
    upstream_request.update_first_party_url_on_redirect = true;
    upstream_request.trusted_params = network::ResourceRequest::TrustedParams();
    upstream_request.trusted_params->isolation_info =
        net::IsolationInfo::Create(
            net::IsolationInfo::RequestType::kMainFrame, upstream_origin,
            upstream_origin, net::SiteForCookies::FromOrigin(upstream_origin));
    upstream_request.mode = network::mojom::RequestMode::kNoCors;
    upstream_request.redirect_mode = network::mojom::RedirectMode::kFollow;
    upstream_request.load_flags =
        original_request.load_flags &
        (net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
         net::LOAD_VALIDATE_CACHE);

    mojo::PendingRemote<network::mojom::URLLoaderClient> upstream_client_remote;
    upstream_client_receiver_.Bind(
        upstream_client_remote.InitWithNewPipeAndPassReceiver());
    upstream_client_receiver_.set_disconnect_handler(base::BindOnce(
        &GlicCicProxyURLLoader::OnUpstreamError, base::Unretained(this)));

    upstream_factory->CreateLoaderAndStart(
        upstream_loader_.BindNewPipeAndPassReceiver(), /*request_id=*/0,
        upstream_options, upstream_request, std::move(upstream_client_remote),
        traffic_annotation);
  }

  ~GlicCicProxyURLLoader() override = default;

  void OnUpstreamError() {
    upstream_loader_.reset();
    upstream_client_receiver_.reset();
    downstream_client_->OnComplete(
        network::URLLoaderCompletionStatus(net::ERR_FAILED));
    Destroy();
  }

  void SendCorpSsoRecoveryResponse() {
    if (type_ ==
        content::ContentBrowserClient::URLLoaderFactoryType::kNavigation) {
      SendSyntheticResponse("200 OK", "text/html; charset=utf-8", "text/html",
                            kSyntheticCsp,
                            BuildCorpSsoRecoveryHtml(cic_base_url_));
    } else {
      SendSyntheticResponse("401 Unauthorized",
                            "application/json; charset=utf-8",
                            "application/json", kSyntheticCsp,
                            BuildCorpSsoExpiredJson(cic_base_url_));
    }
  }

  void SendSyntheticResponse(std::string_view status_line,
                             std::string_view content_type,
                             std::string_view mime_type,
                             std::string_view csp,
                             std::string_view payload) {
    upstream_loader_.reset();
    upstream_client_receiver_.reset();

    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 0;
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    if (mojo::CreateDataPipe(&options, producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      downstream_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
      Destroy();
      return;
    }

    synthetic_payload_size_ = is_head_request_ ? 0 : payload.size();

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
    head->mime_type = std::string(mime_type);
    head->charset = "utf-8";
    head->content_length = static_cast<int64_t>(payload.size());
    head->encoded_data_length = static_cast<int64_t>(synthetic_payload_size_);
    head->headers =
        net::HttpResponseHeaders::Builder({1, 1}, status_line)
            .AddHeader(net::HttpRequestHeaders::kContentType, content_type)
            .AddHeader("Cache-Control", "no-store")
            .AddHeader("Content-Security-Policy", csp)
            .AddHeader("Referrer-Policy", "no-referrer")
            .AddHeader("X-Content-Type-Options", "nosniff")
            .Build();
    head->parsed_headers = network::PopulateParsedHeaders(
        head->headers.get(), original_request_url_);

    downstream_client_->OnReceiveResponse(std::move(head),
                                          std::move(consumer_handle),
                                          /*cached_metadata=*/std::nullopt);

    if (is_head_request_) {
      producer_handle.reset();
      network::URLLoaderCompletionStatus status(net::OK);
      status.encoded_data_length = base::ByteSize(0);
      status.encoded_body_length = base::ByteSize(0);
      status.decoded_body_length = base::ByteSize(0);
      status.completion_time = base::TimeTicks::Now();
      downstream_client_->OnComplete(status);
      Destroy();
      return;
    }

    synthetic_producer_ =
        std::make_unique<mojo::DataPipeProducer>(std::move(producer_handle));
    synthetic_producer_->Write(
        std::make_unique<mojo::StringDataSource>(
            payload, mojo::StringDataSource::AsyncWritingMode::
                         STRING_MAY_BE_INVALIDATED_BEFORE_COMPLETION),
        base::BindOnce(&GlicCicProxyURLLoader::OnSyntheticWriteComplete,
                       base::Unretained(this)));
  }

  void OnSyntheticWriteComplete(MojoResult result) {
    synthetic_producer_.reset();
    if (result != MOJO_RESULT_OK) {
      downstream_client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FAILED));
      Destroy();
      return;
    }
    network::URLLoaderCompletionStatus status(net::OK);
    status.encoded_data_length = base::ByteSize(synthetic_payload_size_);
    status.encoded_body_length = base::ByteSize(synthetic_payload_size_);
    status.decoded_body_length = base::ByteSize(synthetic_payload_size_);
    status.completion_time = base::TimeTicks::Now();
    downstream_client_->OnComplete(status);
    Destroy();
  }

  void Destroy() { delete this; }

  mojo::Receiver<network::mojom::URLLoader> downstream_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> downstream_client_;
  mojo::Remote<network::mojom::URLLoader> upstream_loader_;
  mojo::Receiver<network::mojom::URLLoaderClient> upstream_client_receiver_{
      this};
  std::unique_ptr<mojo::DataPipeProducer> synthetic_producer_;
  size_t synthetic_payload_size_ = 0;

  const GURL original_request_url_;
  const GURL cic_base_url_;
  const url::Origin cic_origin_;
  GURL current_upstream_url_;
  const content::ContentBrowserClient::URLLoaderFactoryType type_;
  const bool is_head_request_;
  int redirect_count_ = 0;
};

}  // namespace

GURL GetGlicCicBaseUrl() {
  const std::string override_url = features::kGlicCicUrl.Get();
  if (!override_url.empty()) {
    GURL parsed(override_url);
    const bool valid_scheme =
        parsed.SchemeIs(url::kHttpsScheme) ||
        (parsed.SchemeIs(url::kHttpScheme) && net::IsLocalhost(parsed));
    if (parsed.is_valid() && valid_scheme && !parsed.has_username() &&
        !parsed.has_password()) {
      return parsed.DeprecatedGetOriginAsURL();
    }
    LOG(WARNING) << "Ignoring invalid GlicCic url feature param: "
                 << override_url;
  }
  return GURL(kProdCicBaseUrl);
}

std::optional<GURL> RewriteGlicCicTargetUrl(
    const GURL& request_url,
    const url::Origin& guest_origin,
    std::string_view guest_path,
    const GURL& cic_base_url,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    bool is_primary_main_frame,
    std::string_view method,
    const std::optional<url::Origin>& request_initiator) {
  if (!request_url.is_valid() || !request_url.SchemeIsHTTPOrHTTPS() ||
      !cic_base_url.is_valid() || guest_origin.opaque()) {
    return std::nullopt;
  }
  if (!url::Origin::Create(request_url).IsSameOriginWith(guest_origin)) {
    return std::nullopt;
  }

  const std::string_view path = request_url.path();
  if (!IsSafeProxyPath(path)) {
    return std::nullopt;
  }

  const size_t cic_pos = path.find(kCicPathSegment);
  if (type ==
      content::ContentBrowserClient::URLLoaderFactoryType::kNavigation) {
    if (!method.empty() && method != net::HttpRequestHeaders::kGetMethod &&
        method != net::HttpRequestHeaders::kHeadMethod) {
      return std::nullopt;
    }
    if (cic_pos != std::string_view::npos) {
      return BuildTargetUrlWithPath(cic_base_url, request_url,
                                    path.substr(cic_pos));
    }
    if (!is_primary_main_frame || !IsGlicDocumentPath(path, guest_path)) {
      return std::nullopt;
    }
    std::string passthrough;
    if (net::GetValueForKeyInQuery(request_url, kGlicPassthroughQueryParam,
                                   &passthrough) &&
        passthrough == "1") {
      return std::nullopt;
    }
    return BuildTargetUrlWithPath(cic_base_url, request_url, "/glic");
  }

  if (type == content::ContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource) {
    if (!request_initiator.has_value() ||
        !request_initiator->IsSameOriginWith(guest_origin) ||
        cic_pos == std::string_view::npos) {
      return std::nullopt;
    }
    return BuildTargetUrlWithPath(cic_base_url, request_url,
                                  path.substr(cic_pos));
  }

  return std::nullopt;
}

GlicCicProxyingURLLoaderFactory::GlicCicProxyingURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    base::WeakPtr<Profile> profile,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    bool is_primary_main_frame,
    url::Origin guest_origin,
    std::string guest_path,
    GURL cic_base_url,
    base::SelfDeletingPassKey pass_key)
    : network::SelfDeletingURLLoaderFactory(std::move(loader_receiver),
                                            pass_key),
      profile_(std::move(profile)),
      type_(type),
      is_primary_main_frame_(is_primary_main_frame),
      guest_origin_(std::move(guest_origin)),
      guest_path_(std::move(guest_path)),
      cic_base_url_(std::move(cic_base_url)) {
  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&GlicCicProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
}

GlicCicProxyingURLLoaderFactory::~GlicCicProxyingURLLoaderFactory() = default;

// static
void GlicCicProxyingURLLoaderFactory::MaybeProxyRequest(
    content::RenderFrameHost* frame,
    content::ContentBrowserClient::URLLoaderFactoryType type,
    const url::Origin& request_initiator,
    network::URLLoaderFactoryBuilder& factory_builder) {
  if (!base::FeatureList::IsEnabled(features::kGlicCic)) {
    return;
  }
  if (type !=
          content::ContentBrowserClient::URLLoaderFactoryType::kNavigation &&
      type != content::ContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource) {
    return;
  }
  bool is_primary_main_frame = false;
  if (!IsInNoWebviewGlicGuestPrimaryPage(frame, &is_primary_main_frame)) {
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

  const GURL guest_url = GetGuestURL(web_contents->GetBrowserContext());
  url::Origin guest_origin = url::Origin::Create(guest_url);
  if (guest_origin.opaque()) {
    return;
  }
  if (type == content::ContentBrowserClient::URLLoaderFactoryType::
                  kDocumentSubResource &&
      !request_initiator.IsSameOriginWith(guest_origin)) {
    return;
  }

  GURL cic_base_url = GetGlicCicBaseUrl();
  if (!cic_base_url.is_valid()) {
    return;
  }

  auto [receiver, remote] = factory_builder.Append();
  base::MakeSelfDeleting<GlicCicProxyingURLLoaderFactory>(
      std::move(receiver), std::move(remote), profile->GetWeakPtr(), type,
      is_primary_main_frame, std::move(guest_origin),
      std::string(guest_url.path()), std::move(cic_base_url));
}

void GlicCicProxyingURLLoaderFactory::SetUpstreamFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> upstream_factory) {
  upstream_factory_for_testing_ = std::move(upstream_factory);
}

void GlicCicProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  std::optional<GURL> target_url = RewriteGlicCicTargetUrl(
      request.url, guest_origin_, guest_path_, cic_base_url_, type_,
      is_primary_main_frame_, request.method, request.request_initiator);
  if (!target_url.has_value()) {
    target_factory_->CreateLoaderAndStart(
        std::move(loader_receiver), request_id, options, request,
        std::move(client), traffic_annotation);
    return;
  }

  if (!profile_ && !upstream_factory_for_testing_) {
    CompleteClientWithError(std::move(client), net::ERR_ABORTED);
    return;
  }

  const std::string method = request.method.empty()
                                 ? net::HttpRequestHeaders::kGetMethod
                                 : request.method;
  if (!net::HttpUtil::IsToken(method) ||
      network::cors::IsForbiddenMethod(method)) {
    if (type_ == content::ContentBrowserClient::URLLoaderFactoryType::
                     kDocumentSubResource) {
      mojo::ReportBadMessage("Invalid HTTP method for Glic CiC proxy");
    }
    CompleteClientWithError(std::move(client), net::ERR_INVALID_ARGUMENT);
    return;
  }

  if (type_ == content::ContentBrowserClient::URLLoaderFactoryType::
                   kDocumentSubResource &&
      !HasAllowedRequestBody(request.request_body.get())) {
    CompleteClientWithError(std::move(client), net::ERR_ACCESS_DENIED);
    return;
  }

  net::HttpRequestHeaders upstream_headers;
  CopyAllowlistedHeaders(request.headers, &upstream_headers);
  if (!network::AreRequestHeadersSafe(upstream_headers)) {
    CompleteClientWithError(std::move(client), net::ERR_INVALID_ARGUMENT);
    return;
  }

  scoped_refptr<network::SharedURLLoaderFactory> upstream_factory =
      upstream_factory_for_testing_
          ? upstream_factory_for_testing_
          : profile_->GetDefaultStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess();
  if (!upstream_factory) {
    CompleteClientWithError(std::move(client), net::ERR_FAILED);
    return;
  }

  const uint32_t upstream_options =
      type_ == content::ContentBrowserClient::URLLoaderFactoryType::kNavigation
          ? (options & network::mojom::kURLLoadOptionSniffMimeType)
          : network::mojom::kURLLoadOptionNone;

  GlicCicProxyURLLoader::Start(std::move(loader_receiver), std::move(client),
                               std::move(upstream_factory), request, method,
                               std::move(upstream_headers), upstream_options,
                               traffic_annotation, cic_base_url_,
                               std::move(*target_url), type_);
}

void GlicCicProxyingURLLoaderFactory::OnTargetFactoryError() {
  DisconnectReceiversAndDestroy();
}

}  // namespace glic
