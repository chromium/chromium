// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"  // nogncheck
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

// The maximum number of retries allowed for GET requests.
constexpr int kMaxRetries = 2;

// DIAL devices are unlikely to expose uPnP functions other than DIAL, so 256KB
// should be more than sufficient.
constexpr int kMaxResponseSizeBytes = 262144;

namespace media_router {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDialUrlFetcherTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("dial_url_fetcher", R"(
        semantics {
          sender: "DIAL"
          description:
            "Chromium sends a request to a device (such as a smart TV) "
            "discovered via the DIAL (Discovery and Launch) protocol to obtain "
            "its device description or app info data. Chromium then uses the "
            "data to determine the capabilities of the device to be used as a "
            "targetfor casting media content."
          trigger:
            "A new or updated device has been discovered via DIAL in the local "
            "network."
          data: "An HTTP GET request."
          destination: OTHER
          destination_other:
            "A device in the local network."
        }
        policy {
          cookies_allowed: NO
          setting:
            "This feature cannot be disabled by settings."
          chrome_policy {
            EnableMediaRouter {
              policy_options {mode: MANDATORY}
              EnableMediaRouter: false
            }
          }
        })");

void BindURLLoaderFactoryReceiverOnUIThread(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  network::mojom::URLLoaderFactory* factory =
      g_browser_process->system_network_context_manager()
          ->GetURLLoaderFactory();
  factory->Clone(std::move(receiver));
}

std::string GetFakeOriginForDialLaunch() {
  // Syntax: package:Google-Chrome.87.Mac-OS-X
  std::string product_name(version_info::GetProductName());
  base::ReplaceChars(product_name, " ", "-", &product_name);
  std::string os_type(version_info::GetOSType());
  base::ReplaceChars(os_type, " ", "-", &os_type);
  return base::StrCat({"package:", product_name, ".",
                       version_info::GetMajorVersionNumber(), ".", os_type});
}

}  // namespace

DialURLFetcher::DialURLFetcher(DialURLFetcher::SuccessCallback success_cb,
                               DialURLFetcher::ErrorCallback error_cb)
    : success_cb_(std::move(success_cb)), error_cb_(std::move(error_cb)) {}

DialURLFetcher::~DialURLFetcher() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

const network::mojom::URLResponseHead* DialURLFetcher::GetResponseHead() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return loader_ ? loader_->ResponseInfo() : nullptr;
}

void DialURLFetcher::Get(const GURL& url, bool set_origin_header) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Start(url, "GET", std::nullopt, kMaxRetries, set_origin_header);
}

void DialURLFetcher::Delete(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Start(url, "DELETE", std::nullopt, 0, true);
}

void DialURLFetcher::Post(const GURL& url,
                          const std::optional<std::string>& post_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Start(url, "POST", post_data, 0, true);
}

void DialURLFetcher::Start(const GURL& url,
                           const std::string& method,
                           const std::optional<std::string>& post_data,
                           int max_retries,
                           bool set_origin_header) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!loader_);

  auto request = std::make_unique<network::ResourceRequest>();
  request->url = url;
  request->method = method;
  // DIAL requests are made by the browser to a fixed set of URLs in response to
  // user actions, not by Web frames.  They require an Origin header, to prevent
  // arbitrary Web frames from issuing site-controlled DIAL requests via Fetch
  // or XHR.  We set a fake Origin that is only used by the browser to satisfy
  // this requirement.  Rather than attempt to coerce this fake origin into a
  // url::Origin, set the header directly.
  if (set_origin_header)
    request->headers.SetHeader("Origin", GetFakeOriginForDialLaunch());

  method_ = method;

  // net::LOAD_BYPASS_PROXY: Proxies almost certainly hurt more cases than they
  //     help.
  // net::LOAD_DISABLE_CACHE: The request should not touch the cache.
  request->load_flags = net::LOAD_BYPASS_PROXY | net::LOAD_DISABLE_CACHE;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  if (saved_request_for_test_) {
    *saved_request_for_test_ = *request;
  }

  loader_ = network::SimpleURLLoader::Create(std::move(request),
                                             kDialUrlFetcherTrafficAnnotation);

  // Allow the fetcher to retry on 5XX responses and ERR_NETWORK_CHANGED.
  if (max_retries > 0) {
    loader_->SetRetryOptions(
        max_retries,
        network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
            network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  }

  // Section 5.4 of the DIAL spec prohibits redirects.
  // In practice, the callback will only get called once, since |loader_| will
  // be deleted.
  loader_->SetOnRedirectCallback(base::BindRepeating(
      &DialURLFetcher::ReportRedirectError, base::Unretained(this)));

  if (post_data)
    loader_->AttachStringForUpload(*post_data, "text/plain");

  StartDownload();
}

void DialURLFetcher::ReportError(const std::string& message) {
  std::move(error_cb_).Run(message, GetHttpResponseCode());
}

std::optional<int> DialURLFetcher::GetHttpResponseCode() const {
  if (GetResponseHead() && GetResponseHead()->headers) {
    int code = GetResponseHead()->headers->response_code();
    return code == -1 ? std::nullopt : std::optional<int>(code);
  }
  return std::nullopt;
}

void DialURLFetcher::ReportRedirectError(
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head,
    std::vector<std::string>* to_be_removed_headers) {
  // Cancel the request.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  loader_.reset();

  ReportError("Redirect not allowed");
}

void DialURLFetcher::StartDownload() {
  // Bind the request to the system URLLoaderFactory obtained on UI thread.
  // Currently this is the only way to guarantee a live URLLoaderFactory.
  // TOOD(mmenke): Figure out a way to do this transparently on IO thread.
  mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
  auto mojo_receiver = loader_factory.BindNewPipeAndPassReceiver();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&BindURLLoaderFactoryReceiverOnUIThread,
                                std::move(mojo_receiver)));
  loader_->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&DialURLFetcher::ProcessResponse, base::Unretained(this)),
      kMaxResponseSizeBytes);
}

void DialURLFetcher::ProcessResponse(std::unique_ptr<std::string> response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int response_code = loader_->NetError();
  if (response_code != net::Error::OK) {
    ReportError(base::StringPrintf("Internal net::Error: %d", response_code));
    return;
  }

  // Response for POST and DELETE may be empty.
  if (!response || (method_ == "GET" && response->empty())) {
    ReportError("Missing or empty response");
    return;
  }

  if (!base::IsStringUTF8(*response)) {
    ReportError("Invalid response encoding");
    return;
  }

  std::move(success_cb_).Run(*response);
}

}  // namespace media_router
