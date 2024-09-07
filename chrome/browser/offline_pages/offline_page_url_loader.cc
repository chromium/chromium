// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_url_loader.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/offline_pages/offline_page_utils.h"
#include "chrome/browser/renderer_host/chrome_navigation_ui_data.h"
#include "components/offline_pages/core/offline_page_feature.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace offline_pages {

namespace {

constexpr uint32_t kBufferSize = 4096;

net::RedirectInfo CreateRedirectInfo(const GURL& redirected_url,
                                     int response_code) {
  net::RedirectInfo redirect_info;
  redirect_info.new_url = redirected_url;
  redirect_info.new_referrer_policy = net::ReferrerPolicy::NO_REFERRER;
  redirect_info.new_method = "GET";
  redirect_info.status_code = response_code;
  redirect_info.new_site_for_cookies =
      net::SiteForCookies::FromUrl(redirect_info.new_url);
  return redirect_info;
}

bool ShouldCreateLoader(const network::ResourceRequest& resource_request) {
  // Ignore the requests not for the main frame.
  if (resource_request.resource_type !=
      static_cast<int>(blink::mojom::ResourceType::kMainFrame))
    return false;

  // Ignore non-http/https requests.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS())
    return false;

  // Ignore requests other than GET.
  if (resource_request.method != "GET")
    return false;

  return true;
}

}  // namespace

// static
std::unique_ptr<OfflinePageURLLoader> OfflinePageURLLoader::Create(
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback) {
  if (ShouldCreateLoader(tentative_resource_request)) {
    return base::WrapUnique(new OfflinePageURLLoader(
        navigation_ui_data, frame_tree_node_id, tentative_resource_request,
        std::move(callback)));
  }

  std::move(callback).Run({});
  return nullptr;
}

OfflinePageURLLoader::OfflinePageURLLoader(
    content::NavigationUIData* navigation_ui_data,
    content::FrameTreeNodeId frame_tree_node_id,
    const network::ResourceRequest& tentative_resource_request,
    content::URLLoaderRequestInterceptor::LoaderCallback callback)
    : navigation_ui_data_(navigation_ui_data),
      frame_tree_node_id_(frame_tree_node_id),
      transition_type_(tentative_resource_request.transition_type),
      loader_callback_(std::move(callback)) {
  // TODO(crbug.com/40590410): Figure out how offline page interception should
  // interact with URLLoaderThrottles. It might be incorrect to use
  // |tentative_resource_request.headers| here, since throttles can rewrite
  // headers between now and when the request handler passed to
  // |loader_callback_| is invoked.
  request_handler_ = std::make_unique<OfflinePageRequestHandler>(
      tentative_resource_request.url, tentative_resource_request.headers, this);
  request_handler_->Start();
}

OfflinePageURLLoader::~OfflinePageURLLoader() {}

void OfflinePageURLLoader::SetTabIdGetterForTesting(
    OfflinePageRequestHandler::Delegate::TabIdGetter tab_id_getter) {
  tab_id_getter_ = tab_id_getter;
}

void OfflinePageURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  NOTREACHED_IN_MIGRATION();
}

void OfflinePageURLLoader::SetPriority(net::RequestPriority priority,
                                       int32_t intra_priority_value) {
  // Ignore: this class doesn't have a concept of priority.
}

void OfflinePageURLLoader::PauseReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void OfflinePageURLLoader::ResumeReadingBodyFromNet() {
  // Ignore: this class doesn't read from network.
}

void OfflinePageURLLoader::FallbackToDefault() {
  std::move(loader_callback_).Run({});
}

void OfflinePageURLLoader::NotifyStartError(int error) {
  std::move(loader_callback_)
      .Run(base::BindOnce(&OfflinePageURLLoader::OnReceiveError,
                          weak_ptr_factory_.GetWeakPtr(), error));
}

void OfflinePageURLLoader::NotifyHeadersComplete(int64_t file_size) {
  std::move(loader_callback_)
      .Run(base::BindOnce(&OfflinePageURLLoader::OnReceiveResponse,
                          weak_ptr_factory_.GetWeakPtr(), file_size));
}

void OfflinePageURLLoader::NotifyReadRawDataComplete(int bytes_read) {
  if (bytes_read < 0) {
    // Negative |bytes_read| is net error code.
    Finish(bytes_read);
    return;
  }
  if (bytes_read == 0) {
    // Zero |bytes_read| means reaching EOF.
    Finish(net::OK);
    return;
  }

  bytes_of_raw_data_to_transfer_ = base::checked_cast<size_t>(bytes_read);
  write_position_ = 0;

  TransferRawData();
}

void OfflinePageURLLoader::TransferRawData() {
  while (true) {
    base::span<const uint8_t> bytes = base::as_bytes(buffer_->span())
                                          .first(bytes_of_raw_data_to_transfer_)
                                          .subspan(write_position_);
    // If all the read data have been transferred, read more.
    if (bytes.empty()) {
      ReadRawData();
      return;
    }

    size_t bytes_written = 0;
    MojoResult result = producer_handle_->WriteData(
        bytes, MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      handle_watcher_->ArmOrNotify();
      return;
    }

    if (result != MOJO_RESULT_OK) {
      Finish(net::ERR_FAILED);
      return;
    }

    write_position_ += bytes_written;
  }
}

void OfflinePageURLLoader::SetOfflinePageNavigationUIData(
    bool is_offline_page) {
  // This method should be called before the response data is received.
  DCHECK(!receiver_.is_bound());

  ChromeNavigationUIData* navigation_data =
      static_cast<ChromeNavigationUIData*>(navigation_ui_data_);
  std::unique_ptr<OfflinePageNavigationUIData> offline_page_data =
      std::make_unique<OfflinePageNavigationUIData>(is_offline_page);
  navigation_data->SetOfflinePageNavigationUIData(std::move(offline_page_data));
}

int OfflinePageURLLoader::GetPageTransition() const {
  return transition_type_;
}

OfflinePageRequestHandler::Delegate::WebContentsGetter
OfflinePageURLLoader::GetWebContentsGetter() const {
  return base::BindRepeating(&content::WebContents::FromFrameTreeNodeId,
                             frame_tree_node_id_);
}

OfflinePageRequestHandler::Delegate::TabIdGetter
OfflinePageURLLoader::GetTabIdGetter() const {
  if (!tab_id_getter_.is_null()) {
    return tab_id_getter_;
  }
  return base::BindRepeating(&OfflinePageUtils::GetTabId);
}

void OfflinePageURLLoader::ReadRawData() {
  int result = request_handler_->ReadRawData(buffer_.get(), kBufferSize);
  // If |result| is not ERR_IO_PENDING, the read data is available immediately.
  // Otherwise, the read is asynchronous and NotifyReadRawDataComplete will
  // be invoked when the read finishes.
  if (result != net::ERR_IO_PENDING)
    NotifyReadRawDataComplete(result);
}

void OfflinePageURLLoader::OnReceiveError(
    int error,
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  client_.Bind(std::move(client));
  Finish(error);
}

void OfflinePageURLLoader::OnReceiveResponse(
    int64_t file_size,
    const network::ResourceRequest& /* resource_request */,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  // TODO(crbug.com/40590410): Figure out how offline page interception should
  // interact with URLLoaderThrottles. It might be incorrect to ignore
  // |resource_request| here, since it's the current request after
  // throttles.
  DCHECK(!receiver_.is_bound());
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &OfflinePageURLLoader::OnMojoDisconnect, weak_ptr_factory_.GetWeakPtr()));
  client_.Bind(std::move(client));

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (mojo::CreateDataPipe(kBufferSize, producer_handle_, consumer_handle) !=
      MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }

  auto response_head = network::mojom::URLResponseHead::New();
  response_head->request_start = base::TimeTicks::Now();
  response_head->response_start = response_head->request_start;

  scoped_refptr<net::HttpResponseHeaders> redirect_headers =
      request_handler_->GetRedirectHeaders();
  if (redirect_headers.get()) {
    std::string redirected_url;
    bool is_redirect = redirect_headers->IsRedirect(&redirected_url);
    DCHECK(is_redirect);
    response_head->headers = redirect_headers;
    response_head->encoded_data_length = 0;
    client_->OnReceiveRedirect(
        CreateRedirectInfo(GURL(redirected_url),
                           redirect_headers->response_code()),
        std::move(response_head));
    return;
  }

  response_head->mime_type = "multipart/related";
  response_head->content_length = file_size;

  client_->OnReceiveResponse(std::move(response_head),
                             std::move(consumer_handle), std::nullopt);

  handle_watcher_ = std::make_unique<mojo::SimpleWatcher>(
      FROM_HERE, mojo::SimpleWatcher::ArmingPolicy::MANUAL,
      base::SequencedTaskRunner::GetCurrentDefault());
  handle_watcher_->Watch(
      producer_handle_.get(), MOJO_HANDLE_SIGNAL_WRITABLE,
      MOJO_WATCH_CONDITION_SATISFIED,
      base::BindRepeating(&OfflinePageURLLoader::OnHandleReady,
                          weak_ptr_factory_.GetWeakPtr()));

  buffer_ = base::MakeRefCounted<net::IOBufferWithSize>(kBufferSize);
  ReadRawData();
}

void OfflinePageURLLoader::OnHandleReady(
    MojoResult result,
    const mojo::HandleSignalsState& state) {
  if (result != MOJO_RESULT_OK) {
    Finish(net::ERR_FAILED);
    return;
  }
  TransferRawData();
}

void OfflinePageURLLoader::Finish(int error) {
  client_->OnComplete(network::URLLoaderCompletionStatus(error));
  handle_watcher_.reset();
  producer_handle_.reset();
  client_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
  MaybeDeleteSelf();
}

void OfflinePageURLLoader::OnMojoDisconnect() {
  receiver_.reset();
  client_.reset();
  MaybeDeleteSelf();
}

void OfflinePageURLLoader::MaybeDeleteSelf() {
  if (!receiver_.is_bound() && !client_.is_bound())
    delete this;
}

}  // namespace offline_pages
