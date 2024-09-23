// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/network_service/aw_proxying_url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "android_webview/browser/android_protocol_handler.h"
#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents_client_bridge.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_contents_origin_matcher.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_settings.h"
#include "android_webview/browser/cookie_manager.h"
#include "android_webview/browser/network_service/aw_web_resource_intercept_response.h"
#include "android_webview/browser/network_service/net_helpers.h"
#include "android_webview/browser/renderer_host/auto_login_parser.h"
#include "android_webview/common/aw_features.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/common/url_constants.h"
#include "base/android/build_info.h"
#include "base/barrier_closure.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/trace_event/base_tracing.h"
#include "components/embedder_support/android/util/input_stream.h"
#include "components/embedder_support/android/util/response_delegate_impl.h"
#include "components/embedder_support/android/util/web_resource_response.h"
#include "components/safe_browsing/core/common/safebrowsing_constants.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/origin_trials_controller_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_inclusion_status.h"
#include "net/http/http_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace android_webview {

namespace {

using PrivacySetting = net::NetworkDelegate::PrivacySetting;

using OptionalGetCookie = std::optional<base::RepeatingCallback<void(
    bool is_3pc_allowed,
    const network::ResourceRequest& request,
    base::OnceCallback<void(std::string)> callback)>>;

using OptionalSetCookie = std::optional<
    embedder_support::AndroidStreamReaderURLLoader::SetCookieHeader>;

std::unique_ptr<AwContentsIoThreadClient> GetIoThreadClient(
    content::FrameTreeNodeId frame_tree_node_id,
    AwBrowserContextIoThreadHandle* browser_context_handle) {
  // |frame_tree_node_id_| is set to be invalid for service workers.
  // |request_.originated_from_service_worker| is insufficient here because it
  // is not set to true on browser side requested main scripts.
  if (frame_tree_node_id.is_null()) {
    return browser_context_handle
               ? browser_context_handle->GetServiceWorkerIoThreadClient()
               : nullptr;
  }
  return AwContentsIoThreadClient::FromID(frame_tree_node_id);
}

const char kResponseHeaderViaShouldInterceptRequestName[] = "Client-Via";
const char kResponseHeaderViaShouldInterceptRequestValue[] =
    "shouldInterceptRequest";
const char kAutoLoginHeaderName[] = "X-Auto-Login";
const char kRequestedWithHeaderWebView[] = "WebView";

// Argument struct for the |InterceptRequest::InterceptResponseReceived| method
// which can live on the heap and be populated by async callbacks.
struct InterceptResponseReceivedArgs {
  std::unique_ptr<AwWebResourceInterceptResponse> intercept_response = nullptr;
  std::unique_ptr<embedder_support::InputStream> input_stream = nullptr;
  bool xrw_origin_trial_enabled = false;
};

// Handles intercepted, in-progress requests/responses, so that they can be
// controlled and modified accordingly.
class InterceptedRequest : public network::mojom::URLLoader,
                           public network::mojom::URLLoaderClient {
 public:
  InterceptedRequest(
      OptionalGetCookie get_cookie_header,
      OptionalSetCookie set_cookie_header,
      content::FrameTreeNodeId frame_tree_node_id,
      int32_t request_id,
      uint32_t options,
      network::ResourceRequest request,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
      bool intercept_only,
      std::optional<AwProxyingURLLoaderFactory::SecurityOptions>
          security_options,
      scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher,
      scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle);

  InterceptedRequest(const InterceptedRequest&) = delete;
  InterceptedRequest& operator=(const InterceptedRequest&) = delete;

  ~InterceptedRequest() override;

  void Restart(std::optional<bool> xrw_enabled);

  // network::mojom::URLLoaderClient
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override;
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override;
  void OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                         network::mojom::URLResponseHeadPtr head) override;
  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback callback) override;
  void OnTransferSizeUpdated(int32_t transfer_size_diff) override;
  void OnComplete(const network::URLLoaderCompletionStatus& status) override;

  // network::mojom::URLLoader
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override;
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override;
  void PauseReadingBodyFromNet() override;
  void ResumeReadingBodyFromNet() override;

  void ContinueAfterIntercept();
  void ContinueAfterInterceptWithOverride(
      std::unique_ptr<embedder_support::WebResourceResponse> response,
      std::unique_ptr<embedder_support::InputStream> input_stream);

  void InterceptResponseReceived(
      std::unique_ptr<InterceptResponseReceivedArgs> args);

  // Returns true if the request was restarted or completed.
  bool InputStreamFailed(bool restart_needed);

  void GetCookieStringOnUI(bool accept_third_party_cookies,
                           base::OnceClosure complete);

  void InterceptWithCookieHeader(
      std::optional<bool> xrw_enabled,
      AwContentsIoThreadClient::ShouldInterceptRequestResponseCallback callback,
      std::string cookie);

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CommittedRequestedWithHeaderMode {
    kNoHeader = 0,
    kAppPackageName = 1,
    kConstantWebview = 2,
    kClientOverridden = 3,
    kMaxValue = kClientOverridden
  };

  std::unique_ptr<AwContentsIoThreadClient> GetIoThreadClient();

  // This is called when the original URLLoaderClient has a connection error.
  void OnURLLoaderClientError();

  // This is called when the original URLLoader has a connection error.
  void OnURLLoaderError(uint32_t custom_reason, const std::string& description);

  // Call OnComplete on |target_client_|. If |wait_for_loader_error| is true
  // then this object will wait for |proxied_loader_receiver_| to have a
  // connection error before destructing.
  void CallOnComplete(const network::URLLoaderCompletionStatus& status,
                      bool wait_for_loader_error);

  void SendErrorAndCompleteImmediately(int error_code);

  // TODO(timvolodine): consider factoring this out of this class.
  bool ShouldNotInterceptRequest();

  // Posts the error callback to the UI thread, ensuring that at most we send
  // only one.
  void SendErrorCallback(int error_code, bool safebrowsing_hit);

  void SendNoIntercept(std::optional<bool> xrw_enabled);

  OptionalGetCookie get_cookie_header_;
  OptionalSetCookie set_cookie_header_;
  const content::FrameTreeNodeId frame_tree_node_id_;
  const int32_t request_id_;
  const uint32_t options_;
  bool input_stream_previously_failed_ = false;
  bool request_was_redirected_ = false;

  // To avoid sending multiple OnReceivedError callbacks.
  bool sent_error_callback_ = false;

  // When true, the loader will not not proceed unless the
  // shouldInterceptRequest callback provided a non-null response.
  bool intercept_only_ = false;

  AwSettings::RequestedWithHeaderMode requested_with_header_mode;

  std::optional<AwProxyingURLLoaderFactory::SecurityOptions> security_options_;

  // If the |target_loader_| called OnComplete with an error this stores it.
  // That way the destructor can send it to OnReceivedError if safe browsing
  // error didn't occur.
  int error_status_ = net::OK;

  network::ResourceRequest request_;

  const net::MutableNetworkTrafficAnnotationTag traffic_annotation_;

  mojo::Receiver<network::mojom::URLLoader> proxied_loader_receiver_;
  mojo::Remote<network::mojom::URLLoaderClient> target_client_;

  mojo::Receiver<network::mojom::URLLoaderClient> proxied_client_receiver_{
      this};
  mojo::Remote<network::mojom::URLLoader> target_loader_;
  mojo::Remote<network::mojom::URLLoaderFactory> target_factory_;
  scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher_;
  scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle_;

  base::WeakPtrFactory<InterceptedRequest> weak_factory_{this};
};

// A ResponseDelegate for responses returned by shouldInterceptRequest.
class InterceptResponseDelegate
    : public embedder_support::ResponseDelegateImpl {
 public:
  InterceptResponseDelegate(
      std::unique_ptr<embedder_support::WebResourceResponse> response,
      base::WeakPtr<InterceptedRequest> request)
      : ResponseDelegateImpl(std::move(response)), request_(request) {}

  // AndroidStreamReaderURLLoader::ResponseDelegate implementation:
  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    embedder_support::ResponseDelegateImpl::AppendResponseHeaders(env, headers);
    // Indicate that the response had been obtained via shouldInterceptRequest.
    headers->SetHeader(kResponseHeaderViaShouldInterceptRequestName,
                       kResponseHeaderViaShouldInterceptRequestValue);
  }

  bool OnInputStreamOpenFailed() override {
    // return true if there is no valid request, meaning it has completed or
    // deleted.
    return request_ ? request_->InputStreamFailed(false /* restart_needed */)
                    : true;
  }

 private:
  base::WeakPtr<InterceptedRequest> request_;
};

// A ResponseDelegate based on top of AndroidProtocolHandler for special
// protocols, such as content://, file:///android_asset, and file:///android_res
// URLs.
class ProtocolResponseDelegate
    : public embedder_support::AndroidStreamReaderURLLoader::ResponseDelegate {
 public:
  ProtocolResponseDelegate(const GURL& url,
                           base::WeakPtr<InterceptedRequest> request)
      : url_(url), request_(request) {}

  std::unique_ptr<embedder_support::InputStream> OpenInputStream(
      JNIEnv* env) override {
    return CreateInputStream(env, url_);
  }

  bool OnInputStreamOpenFailed() override {
    // return true if there is no valid request, meaning it has completed or has
    // been deleted.
    return request_ ? request_->InputStreamFailed(true /* restart_needed */)
                    : true;
  }

  bool GetMimeType(JNIEnv* env,
                   const GURL& url,
                   embedder_support::InputStream* stream,
                   std::string* mime_type) override {
    return GetInputStreamMimeType(env, url, stream, mime_type);
  }

  void GetCharset(JNIEnv* env,
                  const GURL& url,
                  embedder_support::InputStream* stream,
                  std::string* charset) override {
    // TODO: We should probably be getting this from the managed side.
  }

  void AppendResponseHeaders(JNIEnv* env,
                             net::HttpResponseHeaders* headers) override {
    // Indicate that the response had been obtained via shouldInterceptRequest.
    // TODO(jam): why is this added for protocol handler (e.g. content scheme
    // and file resources?). The old path does this as well.
    headers->SetHeader(kResponseHeaderViaShouldInterceptRequestName,
                       kResponseHeaderViaShouldInterceptRequestValue);
  }

 private:
  GURL url_;
  base::WeakPtr<InterceptedRequest> request_;
};

InterceptedRequest::InterceptedRequest(
    OptionalGetCookie get_cookie_header,
    OptionalSetCookie set_cookie_header,
    content::FrameTreeNodeId frame_tree_node_id,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory,
    bool intercept_only,
    std::optional<AwProxyingURLLoaderFactory::SecurityOptions> security_options,
    scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher,
    scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle)
    : get_cookie_header_(get_cookie_header),
      set_cookie_header_(set_cookie_header),
      frame_tree_node_id_(frame_tree_node_id),
      request_id_(request_id),
      options_(options),
      intercept_only_(intercept_only),
      requested_with_header_mode(
          AwSettings::GetDefaultRequestedWithHeaderMode()),
      security_options_(security_options),
      request_(std::move(request)),
      traffic_annotation_(traffic_annotation),
      proxied_loader_receiver_(this, std::move(loader_receiver)),
      target_client_(std::move(client)),
      target_factory_(std::move(target_factory)),
      xrw_allowlist_matcher_(std::move(xrw_allowlist_matcher)),
      browser_context_handle_(std::move(browser_context_handle)) {
  // If there is a client error, clean up the request.
  target_client_.set_disconnect_handler(base::BindOnce(
      &InterceptedRequest::OnURLLoaderClientError, base::Unretained(this)));
  proxied_loader_receiver_.set_disconnect_with_reason_handler(base::BindOnce(
      &InterceptedRequest::OnURLLoaderError, base::Unretained(this)));
}

InterceptedRequest::~InterceptedRequest() {
  if (error_status_ != net::OK)
    SendErrorCallback(error_status_, false);
}

namespace {

// Map holding whether the XRW origin trial is enabled for a navigation.
using XrwEnabledMap = std::map<int64_t, std::pair<GURL, bool>>;
XrwEnabledMap& GetXrwEnabledMap() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  static base::NoDestructor<XrwEnabledMap> map;
  return *map;
}

// Persistent Origin Trials can only be checked on the UI thread.
bool CheckXrwOriginTrial(const GURL& request_url,
                         content::FrameTreeNodeId frame_tree_node_id,
                         blink::mojom::ResourceType resource_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::OriginTrialsControllerDelegate* delegate =
      AwBrowserContext::GetDefault()->GetOriginTrialsControllerDelegate();
  if (!delegate)
    return false;

  // Use the request URL for main frame resources (main frame navigation).
  // Use last committed origin of outermost main frame for all other requests.
  // Fall back to an opaque origin if neither is available (not expected to
  // happen).
  url::Origin partition_origin;
  if (resource_type == blink::mojom::ResourceType::kMainFrame) {
    partition_origin = url::Origin::Create(request_url);
  } else {
    content::WebContents* wc =
        content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
    base::UmaHistogramBoolean(
        "Android.WebView.RequestedWithHeader.HadWebContentsForPartitionOrigin",
        wc);
    if (wc) {
      partition_origin = wc->GetPrimaryMainFrame()->GetLastCommittedOrigin();
    }
  }

  bool xrw_origin_trial_enabled = delegate->IsFeaturePersistedForOrigin(
      url::Origin::Create(request_url), partition_origin,
      blink::mojom::OriginTrialFeature::kWebViewXRequestedWithDeprecation,
      base::Time::Now());
  base::UmaHistogramBoolean(
      "Android.WebView.RequestedWithHeader.OriginTrialEnabled",
      xrw_origin_trial_enabled);

  if (xrw_origin_trial_enabled &&
      (request_url.SchemeIsHTTPOrHTTPS() || request_url.SchemeIsWSOrWSS())) {
    base::UmaHistogramBoolean(
        "Android.WebView.RequestedWithHeader.PageSchemeIsCryptographic",
        request_url.SchemeIsCryptographic());
  }
  return xrw_origin_trial_enabled;
}

// Persistent Origin Trials can only be checked on the UI thread.
// |result_args| is owned by a BarrierClosure that executes after this call.
void CheckXrwOriginTrialOnUiThread(GURL request_url,
                                   content::FrameTreeNodeId frame_tree_node_id,
                                   blink::mojom::ResourceType resource_type,
                                   InterceptResponseReceivedArgs* result_args) {
  result_args->xrw_origin_trial_enabled =
      CheckXrwOriginTrial(request_url, frame_tree_node_id, resource_type);
}

// Post a call to the UI thread to check if the XRW deprecation trial is enabled
// for |request_url|, saving the result in |result_args|.
// |result_args| is owned by the |done_callback|. If |cached_result| is present,
// will call |done_callback| synchronously.
void CheckXrwOriginTrialAsync(std::optional<bool> cached_result,
                              GURL request_url,
                              content::FrameTreeNodeId frame_tree_node_id,
                              blink::mojom::ResourceType resource_type,
                              InterceptResponseReceivedArgs* result_args,
                              base::OnceClosure done_callback) {
  if (cached_result) {
    result_args->xrw_origin_trial_enabled = *cached_result;
    std::move(done_callback).Run();
    return;
  }

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&CheckXrwOriginTrialOnUiThread, std::move(request_url),
                     frame_tree_node_id, resource_type,
                     base::Unretained(result_args)),
      std::move(done_callback));
}

// Response callback for |AwContentsIoThreadClient::ShouldInterceptRequestAsync|
// which saves the result to |result_args|, which is owned by the
// |done_callback|.
// |async_result| is last argument to allow currying through bind.
void OnShouldInterceptRequestAsyncResult(
    InterceptResponseReceivedArgs* result_args,
    base::OnceClosure done_closure,
    AwContentsIoThreadClient::InterceptResponseData async_result) {
  result_args->intercept_response = std::move(async_result.response);
  result_args->input_stream = std::move(async_result.input_stream);
  std::move(done_closure).Run();
}

}  // namespace

void InterceptedRequest::InterceptWithCookieHeader(
    std::optional<bool> xrw_enabled,
    AwContentsIoThreadClient::ShouldInterceptRequestResponseCallback callback,
    std::string cookie) {
  if (cookie != "") {
    request_.headers.SetHeader(net::HttpRequestHeaders::kCookie, cookie);
  }

  std::unique_ptr<AwContentsIoThreadClient> io_thread_client =
      GetIoThreadClient();

  if (io_thread_client != nullptr) {
    // TODO: verify the case when WebContents::RenderFrameDeleted is called
    // before network request is intercepted (i.e. if that's possible and
    // whether it can result in any issues).
    io_thread_client->ShouldInterceptRequestAsync(
        AwWebResourceRequest(request_), std::move(callback));
  } else {
    SendNoIntercept(xrw_enabled);
  }
}

void InterceptedRequest::Restart(std::optional<bool> xrw_enabled) {
  TRACE_EVENT0("android_webview", "InterceptedRequest::Restart");
  std::unique_ptr<AwContentsIoThreadClient> io_thread_client =
      GetIoThreadClient();

  if (ShouldBlockURL(request_.url, io_thread_client.get())) {
    SendErrorAndCompleteImmediately(net::ERR_ACCESS_DENIED);
    return;
  }

  if (requested_with_header_mode != AwSettings::APP_PACKAGE_NAME &&
      xrw_allowlist_matcher_ &&
      xrw_allowlist_matcher_->MatchesOrigin(
          url::Origin::Create(request_.url))) {
    requested_with_header_mode = AwSettings::APP_PACKAGE_NAME;
  }

  request_.load_flags =
      UpdateLoadFlags(request_.load_flags, io_thread_client.get());

  if (!io_thread_client || ShouldNotInterceptRequest()) {
    SendNoIntercept(xrw_enabled);
  } else {
    if (request_.referrer.is_valid()) {
      // intentionally override if referrer header already exists
      request_.headers.SetHeader(net::HttpRequestHeaders::kReferer,
                                 request_.referrer.spec());
    }

    base::RepeatingClosure arg_ready_closure;
    // Pointer lifetime is tied to |arg_ready_closure|.
    InterceptResponseReceivedArgs* intercept_response_received_args;
    {
      // Inner scope to prevent |call_args| from being accidentally used after
      // being moved into the |arg_ready_closure|.
      std::unique_ptr<InterceptResponseReceivedArgs> call_args =
          std::make_unique<InterceptResponseReceivedArgs>();
      intercept_response_received_args = call_args.get();
      arg_ready_closure = base::BarrierClosure(
          2, base::BindOnce(&InterceptedRequest::InterceptResponseReceived,
                            weak_factory_.GetWeakPtr(), std::move(call_args)));
    }

    CheckXrwOriginTrialAsync(
        xrw_enabled, request_.url, frame_tree_node_id_,
        static_cast<blink::mojom::ResourceType>(request_.resource_type),
        intercept_response_received_args, arg_ready_closure);

    auto done = base::BindOnce(
        &InterceptedRequest::InterceptWithCookieHeader, base::Unretained(this),
        xrw_enabled,
        base::BindOnce(&OnShouldInterceptRequestAsyncResult,
                       base::Unretained(intercept_response_received_args),
                       arg_ready_closure));

    if (get_cookie_header_.has_value() && io_thread_client &&
        io_thread_client->ShouldAcceptCookies()) {
      bool accept_third_party_cookies =
          io_thread_client->ShouldAcceptThirdPartyCookies();

      std::move(get_cookie_header_)
          ->Run(accept_third_party_cookies, request_, std::move(done));
    } else {
      std::move(done).Run("");
    }
  }
}

// logic for when not to invoke shouldInterceptRequest callback
bool InterceptedRequest::ShouldNotInterceptRequest() {
  if (request_was_redirected_)
    return true;

  // Do not call shouldInterceptRequest callback for special android urls,
  // unless they fail to load on first attempt. Special android urls are urls
  // such as "file:///android_asset/", "file:///android_res/" urls or
  // "content:" scheme urls.
  return !input_stream_previously_failed_ &&
         (request_.url.SchemeIs(url::kContentScheme) ||
          android_webview::IsAndroidSpecialFileUrl(request_.url));
}

void InterceptedRequest::InterceptResponseReceived(
    std::unique_ptr<InterceptResponseReceivedArgs> args) {
  DCHECK(args);
  if (args->xrw_origin_trial_enabled) {
    requested_with_header_mode =
        AwSettings::RequestedWithHeaderMode::APP_PACKAGE_NAME;
  }

  // We send the application's package name in the X-Requested-With header for
  // compatibility with previous WebView versions. This should not be visible to
  // shouldInterceptRequest. It should also not trigger CORS prefetch if
  // OOR-CORS is enabled.
  std::string header = content::GetCorsExemptRequestedWithHeaderName();

  CommittedRequestedWithHeaderMode committed_mode =
      CommittedRequestedWithHeaderMode::kClientOverridden;

  // Only overwrite if the header hasn't already been set
  if (!request_.headers.HasHeader(header)) {
    switch (requested_with_header_mode) {
      case AwSettings::RequestedWithHeaderMode::NO_HEADER:
        committed_mode = CommittedRequestedWithHeaderMode::kNoHeader;
        break;
      case AwSettings::RequestedWithHeaderMode::APP_PACKAGE_NAME:
        request_.cors_exempt_headers.SetHeader(
            header,
            base::android::BuildInfo::GetInstance()->host_package_name());
        committed_mode = CommittedRequestedWithHeaderMode::kAppPackageName;
        break;
      case AwSettings::RequestedWithHeaderMode::CONSTANT_WEBVIEW:
        request_.cors_exempt_headers.SetHeader(header,
                                               kRequestedWithHeaderWebView);
        committed_mode = CommittedRequestedWithHeaderMode::kConstantWebview;
        break;
      default:
        NOTREACHED()
            << "Invalid enum value for AwSettings:RequestedWithHeaderMode: "
            << requested_with_header_mode;
    }
  }
  base::UmaHistogramEnumeration(
      "Android.WebView.RequestedWithHeader.CommittedHeaderMode",
      committed_mode);

  JNIEnv* env = base::android::AttachCurrentThread();
  if (args->intercept_response &&
      args->intercept_response->RaisedException(env)) {
    // The JNI handler has already raised an exception. Fail the resource load
    // as it may be insecure to load on error.
    SendErrorAndCompleteImmediately(net::ERR_UNEXPECTED);
    return;
  }

  if (args->intercept_response && args->intercept_response->HasResponse(env)) {
    // non-null response: make sure to use it as an override for the
    // normal network data.
    ContinueAfterInterceptWithOverride(
        args->intercept_response->GetResponse(env),
        std::move(args->input_stream));
    return;
  }

  // Request was not intercepted/overridden. Proceed with loading from network,
  // unless this is a special |intercept_only_| loader, which happens for
  // external schemes: e.g. unsupported schemes and cid: schemes.
  if (intercept_only_) {
    SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
    return;
  }

  ContinueAfterIntercept();
}

// returns true if the request has been restarted or was completed.
bool InterceptedRequest::InputStreamFailed(bool restart_needed) {
  DCHECK(!input_stream_previously_failed_);

  if (intercept_only_) {
    // This can happen for unsupported schemes, when no proper
    // response from shouldInterceptRequest() is received, i.e.
    // the provided input stream in response failed to load. In
    // this case we send and error and stop loading.
    SendErrorAndCompleteImmediately(net::ERR_UNKNOWN_URL_SCHEME);
    return true;  // request completed
  }

  if (!restart_needed) {
    // request will not be restarted, error reporting will be done
    // via other means e.g. setting appropriate response header status.
    return false;
  }

  input_stream_previously_failed_ = true;
  proxied_client_receiver_.reset();
  Restart(std::nullopt);
  return true;  // request restarted
}

void InterceptedRequest::ContinueAfterIntercept() {
  // For WebViewClassic compatibility this job can only accept URLs that can be
  // opened. URLs that cannot be opened should be resolved by the next handler.
  //
  // If a request is initially handled here but the job fails due to it being
  // unable to open the InputStream for that request the request is marked as
  // previously failed and restarted.
  // Restarting a request involves creating a new job for that request. This
  // handler will ignore requests known to have previously failed to 1) prevent
  // an infinite loop, 2) ensure that the next handler in line gets the
  // opportunity to create a job for the request.
  if (!input_stream_previously_failed_ &&
      (request_.url.SchemeIs(url::kContentScheme) ||
       android_webview::IsAndroidSpecialFileUrl(request_.url))) {
    embedder_support::AndroidStreamReaderURLLoader* loader =
        new embedder_support::AndroidStreamReaderURLLoader(
            request_, proxied_client_receiver_.BindNewPipeAndPassRemote(),
            traffic_annotation_,
            std::make_unique<ProtocolResponseDelegate>(
                request_.url, weak_factory_.GetWeakPtr()),
            security_options_, set_cookie_header_);
    loader->Start(nullptr);
    return;
  }

  if (!target_loader_ && target_factory_) {
    target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, proxied_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
  }
}

void InterceptedRequest::ContinueAfterInterceptWithOverride(
    std::unique_ptr<embedder_support::WebResourceResponse> response,
    std::unique_ptr<embedder_support::InputStream> input_stream) {
  embedder_support::AndroidStreamReaderURLLoader* loader =
      new embedder_support::AndroidStreamReaderURLLoader(
          request_, proxied_client_receiver_.BindNewPipeAndPassRemote(),
          traffic_annotation_,
          std::make_unique<InterceptResponseDelegate>(
              std::move(response), weak_factory_.GetWeakPtr()),
          std::nullopt, set_cookie_header_);
  loader->Start(std::move(input_stream));
}

namespace {
// TODO(timvolodine): consider factoring this out of this file.

AwContentsClientBridge* GetAwContentsClientBridgeFromID(
    content::FrameTreeNodeId frame_tree_node_id) {
  content::WebContents* wc =
      content::WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  return AwContentsClientBridge::FromWebContents(wc);
}

void OnReceivedHttpErrorOnUiThread(
    content::FrameTreeNodeId frame_tree_node_id,
    const AwWebResourceRequest& request,
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> http_error_info) {
  auto* client = GetAwContentsClientBridgeFromID(frame_tree_node_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, std::move(http_error_info));
}

void OnReceivedErrorOnUiThread(content::FrameTreeNodeId frame_tree_node_id,
                               const AwWebResourceRequest& request,
                               int error_code,
                               bool safebrowsing_hit) {
  auto* client = GetAwContentsClientBridgeFromID(frame_tree_node_id);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedError(request, error_code, safebrowsing_hit, true);
}

void OnNewLoginRequestOnUiThread(content::FrameTreeNodeId frame_tree_node_id,
                                 const std::string& realm,
                                 const std::string& account,
                                 const std::string& args) {
  auto* client = GetAwContentsClientBridgeFromID(frame_tree_node_id);
  if (!client) {
    return;
  }
  client->NewLoginRequest(realm, account, args);
}

}  // namespace

// URLLoaderClient methods.

void InterceptedRequest::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  target_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void InterceptedRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT0("android_webview", "InterceptedRequest::OnReceiveResponse");
  // intercept response headers here
  // pause/resume |proxied_client_receiver_| if necessary

  if (head->headers && head->headers->response_code() >= 400) {
    // In Android WebView the WebViewClient.onReceivedHttpError callback
    // is invoked for any resource (main page, iframe, image, etc.) with
    // status code >= 400.
    std::unique_ptr<AwContentsClientBridge::HttpErrorInfo> error_info =
        AwContentsClientBridge::ExtractHttpErrorInfo(head->headers.get());

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&OnReceivedHttpErrorOnUiThread, frame_tree_node_id_,
                       AwWebResourceRequest(request_), std::move(error_info)));
  }

  if (request_.destination == network::mojom::RequestDestination::kDocument) {
    // Check for x-auto-login-header
    HeaderData header_data;
    std::string header_string;
    if (head->headers && head->headers->GetNormalizedHeader(
                             kAutoLoginHeaderName, &header_string)) {
      if (ParseHeader(header_string, ALLOW_ANY_REALM, &header_data)) {
        // TODO(timvolodine): consider simplifying this and above callback
        // code, crbug.com/897149.
        content::GetUIThreadTaskRunner({})->PostTask(
            FROM_HERE, base::BindOnce(&OnNewLoginRequestOnUiThread,
                                      frame_tree_node_id_, header_data.realm,
                                      header_data.account, header_data.args));
      }
    }
  }

  target_client_->OnReceiveResponse(std::move(head), std::move(body),
                                    std::move(cached_metadata));
}

void InterceptedRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  // TODO(timvolodine): handle redirect override.
  request_was_redirected_ = true;
  target_client_->OnReceiveRedirect(redirect_info, std::move(head));
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

void InterceptedRequest::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kInterceptedRequest);
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void InterceptedRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  // Only wait for the original loader to possibly have a custom error if the
  // target loader succeeded. If the target loader failed, then it was a race as
  // to whether that error or the safe browsing error would be reported.
  CallOnComplete(status, status.error_code == net::OK);
}

// URLLoader methods.

void InterceptedRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  if (target_loader_) {
    target_loader_->FollowRedirect(removed_headers, modified_headers,
                                   modified_cors_exempt_headers, new_url);
  }

  // If |OnURLLoaderClientError| was called then we're just waiting for the
  // connection error handler of |proxied_loader_receiver_|. Don't restart the
  // job since that'll create another URLLoader
  if (!target_client_)
    return;

  Restart(std::nullopt);
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

std::unique_ptr<AwContentsIoThreadClient>
InterceptedRequest::GetIoThreadClient() {
  return ::android_webview::GetIoThreadClient(frame_tree_node_id_,
                                              browser_context_handle_.get());
}

void InterceptedRequest::OnURLLoaderClientError() {
  // We set |wait_for_loader_error| to true because if the loader did have a
  // custom_reason error then the client would be reset as well and it would be
  // a race as to which connection error we saw first.
  CallOnComplete(network::URLLoaderCompletionStatus(net::ERR_ABORTED),
                 true /* wait_for_loader_error */);
}

void InterceptedRequest::OnURLLoaderError(uint32_t custom_reason,
                                          const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason) {
    if (description == safe_browsing::kCustomCancelReasonForURLLoader) {
      SendErrorCallback(safe_browsing::kNetErrorCodeForSafeBrowsing, true);
    } else {
      int parsed_error_code;
      if (base::StringToInt(std::string_view(description),
                            &parsed_error_code)) {
        SendErrorCallback(parsed_error_code, false);
      }
    }
  }

  // If CallOnComplete was already called, then this object is ready to be
  // deleted.
  if (!target_client_)
    delete this;
}

void InterceptedRequest::CallOnComplete(
    const network::URLLoaderCompletionStatus& status,
    bool wait_for_loader_error) {
  // Save an error status so that we call onReceiveError at destruction if there
  // was no safe browsing error.
  if (status.error_code != net::OK)
    error_status_ = status.error_code;

  if (target_client_)
    target_client_->OnComplete(status);

  if (proxied_loader_receiver_.is_bound() && wait_for_loader_error) {
    // Since the original client is gone no need to continue loading the
    // request.
    proxied_client_receiver_.reset();
    target_loader_.reset();

    // Don't delete |this| yet, in case the |proxied_loader_receiver_|'s
    // error_handler is called with a reason to indicate an error which we want
    // to send to the client bridge. Also reset |target_client_| so we don't
    // get its error_handler called and then delete |this|.
    target_client_.reset();

    // In case there are pending checks as to whether this request should be
    // intercepted, we don't want that causing |target_client_| to be used
    // later.
    weak_factory_.InvalidateWeakPtrs();
  } else {
    delete this;
  }
}

void InterceptedRequest::SendErrorAndCompleteImmediately(int error_code) {
  auto status = network::URLLoaderCompletionStatus(error_code);
  SendErrorCallback(status.error_code, false);
  target_client_->OnComplete(status);
  delete this;
}

void InterceptedRequest::SendErrorCallback(int error_code,
                                           bool safebrowsing_hit) {
  // Ensure we only send one error callback, e.g. to avoid sending two if
  // there's both a networking error and safe browsing blocked the request.
  if (sent_error_callback_)
    return;

  // We can't get a |AwContentsClientBridge| based on the |render_frame_id| of
  // the |request_| initiated by the service worker, so interrupt it as soon as
  // possible.
  if (request_.originated_from_service_worker)
    return;

  sent_error_callback_ = true;
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&OnReceivedErrorOnUiThread, frame_tree_node_id_,
                                AwWebResourceRequest(request_), error_code,
                                safebrowsing_hit));
}

void InterceptedRequest::SendNoIntercept(std::optional<bool> xrw_enabled) {
  // equivalent to no interception
  std::unique_ptr<InterceptResponseReceivedArgs>
      intercept_response_received_args =
          std::make_unique<InterceptResponseReceivedArgs>();

  CheckXrwOriginTrialAsync(
      xrw_enabled, request_.url, frame_tree_node_id_,
      static_cast<blink::mojom::ResourceType>(request_.resource_type),
      intercept_response_received_args.get(),
      base::BindOnce(&InterceptedRequest::InterceptResponseReceived,
                     weak_factory_.GetWeakPtr(),
                     std::move(intercept_response_received_args)));
}

}  // namespace

//============================
// AwProxyingURLLoaderFactory
//============================

AwProxyingURLLoaderFactory::AwProxyingURLLoaderFactory(
    std::optional<mojo::PendingRemote<network::mojom::CookieManager>>
        cookie_manager,
    AwCookieAccessPolicy* cookie_access_policy,
    std::optional<const net::IsolationInfo> isolation_info,
    content::FrameTreeNodeId frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    bool intercept_only,
    std::optional<SecurityOptions> security_options,
    scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher,
    scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle,
    std::optional<int64_t> navigation_id)
    : cookie_access_policy_(cookie_access_policy),
      isolation_info_(isolation_info),
      frame_tree_node_id_(frame_tree_node_id),
      intercept_only_(intercept_only),
      security_options_(security_options),
      xrw_allowlist_matcher_(std::move(xrw_allowlist_matcher)),
      browser_context_handle_(std::move(browser_context_handle)),
      navigation_id_(navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!(intercept_only_ && target_factory_remote));
  if (target_factory_remote) {
    target_factory_.Bind(std::move(target_factory_remote));
    target_factory_.set_disconnect_handler(
        base::BindOnce(&AwProxyingURLLoaderFactory::OnTargetFactoryError,
                       base::Unretained(this)));
  }
  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(
      base::BindRepeating(&AwProxyingURLLoaderFactory::OnProxyBindingError,
                          base::Unretained(this)));

  if (cookie_manager.has_value() && cookie_manager->is_valid()) {
    cookie_manager_.Bind(std::move(cookie_manager.value()));
  }
}

AwProxyingURLLoaderFactory::~AwProxyingURLLoaderFactory() = default;

// static
void AwProxyingURLLoaderFactory::SetXrwResultForNavigation(
    const GURL& url,
    blink::mojom::ResourceType resource_type,
    content::FrameTreeNodeId frame_tree_node_id,
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool result = CheckXrwOriginTrial(url, frame_tree_node_id, resource_type);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](int64_t navigation_id, GURL url, bool result) {
                       GetXrwEnabledMap()[navigation_id] =
                           std::make_pair(url, result);
                     },
                     navigation_id, url, result));
}

// static
void AwProxyingURLLoaderFactory::ClearXrwResultForNavigation(
    int64_t navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(
                     [](int64_t navigation_id) {
                       GetXrwEnabledMap().erase(navigation_id);
                     },
                     navigation_id));
}

// static
void AwProxyingURLLoaderFactory::CreateProxy(
    mojo::PendingRemote<network::mojom::CookieManager> cookie_manager,
    AwCookieAccessPolicy* cookie_access_policy,
    std::optional<const net::IsolationInfo> isolation_info,
    content::FrameTreeNodeId frame_tree_node_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    std::optional<SecurityOptions> security_options,
    scoped_refptr<AwContentsOriginMatcher> xrw_allowlist_matcher,
    scoped_refptr<AwBrowserContextIoThreadHandle> browser_context_handle,
    std::optional<int64_t> navigation_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // will manage its own lifetime
  new AwProxyingURLLoaderFactory(
      std::move(cookie_manager), cookie_access_policy, isolation_info,
      frame_tree_node_id, std::move(loader_receiver),
      std::move(target_factory_remote), false, security_options,
      std::move(xrw_allowlist_matcher), std::move(browser_context_handle),
      navigation_id);
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  NOTREACHED() << "Non-const ref version of this method should be used as a "
                  "performance optimization.";
}

void AwProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    int32_t request_id,
    uint32_t options,
    network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  TRACE_EVENT0("android_webview",
               "AwProxyingURLLoaderFactory::CreateLoaderAndStart");
  // TODO(timvolodine): handle interception, modification (headers for
  // webview), blocking, callbacks etc..

  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_clone;
  if (target_factory_) {
    target_factory_->Clone(
        target_factory_clone.InitWithNewPipeAndPassReceiver());
  }

  std::unique_ptr<AwContentsIoThreadClient> io_thread_client =
      GetIoThreadClient(frame_tree_node_id_, browser_context_handle_.get());

  // It is possible for us to receive a nullptr for the io_thread_client
  // from AwContentBrowserClient::HandleExternalProtocol.
  // This is because that method can be called while the RenderFrameHost is
  // shutting down. Since this behavior is only expected during shutdown, we
  // will take the safe default and assume cookies are not allowed to avoid
  // leaking data.
  bool global_cookie_policy = io_thread_client != nullptr
                                  ? io_thread_client->ShouldAcceptCookies()
                                  : false;

  bool third_party_cookie_policy =
      global_cookie_policy && io_thread_client->ShouldAcceptThirdPartyCookies();

  if (!global_cookie_policy) {
    options |= network::mojom::kURLLoadOptionBlockAllCookies;
  } else if (!third_party_cookie_policy && !request.url.SchemeIsFile()) {
    // Special case: if the application has asked that we allow file:// scheme
    // URLs to set cookies, we need to avoid setting a cookie policy (as file://
    // scheme URLs are third-party to everything).
    options |= network::mojom::kURLLoadOptionBlockThirdPartyCookies;
  }

  std::optional<bool> xrw_enabled;
  if (navigation_id_) {
    auto& map = GetXrwEnabledMap();
    auto it = map.find(*navigation_id_);
    if (it != map.end()) {
      // Make sure the origin still matches for this navigation.
      if (url::Origin::Create(it->second.first) ==
          url::Origin::Create(request.url)) {
        xrw_enabled = it->second.second;
      }
      map.erase(it);
    }
  }

  // If we are handling an external protocol, we skip providing the cookie
  // manager. In this case, it will not be bound so we move on.
  OptionalGetCookie get_cookie_header = std::nullopt;
  OptionalSetCookie set_cookie_header = std::nullopt;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kWebViewInterceptedCookieHeader) &&
      cookie_manager_.is_bound()) {
    get_cookie_header = base::BindRepeating(
        &AwProxyingURLLoaderFactory::GetCookieHeader, base::Unretained(this));
    set_cookie_header = base::BindRepeating(
        &AwProxyingURLLoaderFactory::SetCookieHeader, base::Unretained(this));
  }

  // manages its own lifecycle
  // TODO(timvolodine): consider keeping track of requests.
  InterceptedRequest* req;
  if (base::FeatureList::IsEnabled(
          network::features::kAvoidResourceRequestCopies)) {
    // TODO(crbug.com/332697604): Pass by non-const ref once mojo supports it.
    req = new InterceptedRequest(
        std::move(get_cookie_header), std::move(set_cookie_header),
        frame_tree_node_id_, request_id, options, std::move(request),
        traffic_annotation, std::move(loader), std::move(client),
        std::move(target_factory_clone), intercept_only_, security_options_,
        xrw_allowlist_matcher_, browser_context_handle_);
  } else {
    req = new InterceptedRequest(
        std::move(get_cookie_header), std::move(set_cookie_header),
        frame_tree_node_id_, request_id, options, request, traffic_annotation,
        std::move(loader), std::move(client), std::move(target_factory_clone),
        intercept_only_, security_options_, xrw_allowlist_matcher_,
        browser_context_handle_);
  }
  req->Restart(xrw_enabled);
}

void AwProxyingURLLoaderFactory::OnTargetFactoryError() {
  delete this;
}

void AwProxyingURLLoaderFactory::OnProxyBindingError() {
  if (proxy_receivers_.empty())
    delete this;
}

std::optional<net::CookiePartitionKey> GetPartitionKey(
    net::IsolationInfo& isolation_info,
    const network::ResourceRequest& request) {
  return net::CookiePartitionKey::FromNetworkIsolationKey(
      isolation_info.network_isolation_key(), isolation_info.site_for_cookies(),
      net::SchemefulSite(request.url), request.is_outermost_main_frame);
}

// We need to use this function to get the cookie header for Android apps
// because we are letting them intercept requests before we have even handed
// over the network request to the actual network stack.
void AwProxyingURLLoaderFactory::GetCookieHeader(
    bool is_3pc_allowed,
    const network::ResourceRequest& request,
    base::OnceCallback<void(std::string)> callback) {
  DCHECK(cookie_manager_.is_bound() && cookie_access_policy_ != nullptr);

  auto isolation_info = GetIsolationInfo(request);

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();

  net::SchemefulSite site_to_partition =
      isolation_info.network_isolation_key().GetTopFrameSite().value_or(
          net::SchemefulSite());

  PrivacySetting privacy_setting = cookie_access_policy_->CanAccessCookies(
      request.url, isolation_info.site_for_cookies(), is_3pc_allowed,
      request.storage_access_api_status);

  // We should not bother retrieving the cookie list if cookies are not enabled.
  if (privacy_setting == PrivacySetting::kStateDisallowed) {
    std::move(callback).Run("");
    return;
  }

  cookie_manager_->GetCookieList(
      request.url, options,
      net::CookiePartitionKeyCollection::FromOptional(
          GetPartitionKey(isolation_info, request)),
      base::BindOnce(
          [](PrivacySetting privacy_setting,
             base::OnceCallback<void(std::string)> callback,
             const net::CookieAccessResultList& results,
             const net::CookieAccessResultList& excluded_cookies) {
            net::CookieList cookies;

            for (const net::CookieWithAccessResult& cookie : results) {
              if (privacy_setting == PrivacySetting::kStateAllowed ||
                  cookie.cookie.IsPartitioned()) {
                cookies.push_back(cookie.cookie);
              }
            }

            std::move(callback).Run(
                net::CanonicalCookie::BuildCookieLine(cookies));
          },
          std::move(privacy_setting), std::move(callback)));
}

void AwProxyingURLLoaderFactory::SetCookieHeader(
    const network::ResourceRequest& request,
    const std::string& cookie_string,
    const std::optional<base::Time>& server_time) {
  DCHECK(cookie_manager_.is_bound());
  auto isolation_info = GetIsolationInfo(request);

  net::CookieInclusionStatus returned_status;

  std::unique_ptr<net::CanonicalCookie> cookie = net::CanonicalCookie::Create(
      request.url, cookie_string, base::Time::Now(), server_time,
      GetPartitionKey(isolation_info, request), net::CookieSourceType::kHTTP,
      &returned_status);

  cookie_manager_->SetCanonicalCookie(*cookie, request.url,
                                      net::CookieOptions::MakeAllInclusive(),
                                      base::DoNothing());
}

net::IsolationInfo AwProxyingURLLoaderFactory::GetIsolationInfo(
    const network::ResourceRequest& request) {
  CHECK(isolation_info_.has_value());
  // If the factory is trusted, this will be included, otherwise we
  // receive the isolation info from WillCreateURLLoaderFactory when we
  // are being created.
  // See the WillCreateURLLoaderFactory doc block for more info on this.
  if (request.trusted_params.has_value()) {
    return request.trusted_params->isolation_info;
  }

  return isolation_info_.value();
}

void AwProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

}  // namespace android_webview
