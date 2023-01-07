// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_diagnostics/http_request_manager.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"

namespace ash {
namespace network_diagnostics {
namespace {

// Maximum number of retries for sending the request.
constexpr int kMaxRetries = 2;
// HTTP Get method.
const char kGetMethod[] = "GET";

net::NetworkTrafficAnnotationTag GetTrafficAnnotation() {
  return net::DefineNetworkTrafficAnnotation("network_diagnostics_routines",
                                             R"(
      semantics {
        sender: "NetworkDiagnosticsRoutines"
        description: "Routines send network traffic (http requests) to "
          "hosts in order to validate the internet connection on a device."
        trigger: "A routine makes an http request."
        data:
          "No data other than the path is sent. No user identifier is "
          "sent along with the data."
        destination: WEBSITE
      }
      policy {
        cookies_allowed: NO
        policy_exception_justification:
          "No policy defined to enable/disable or limit this request as this "
          "is on-demand user initiated operation to do the network diagnostics."
      }
  )");
}

}  // namespace

HttpRequestManager::HttpRequestManager(Profile* profile) {
  // |profile| may be null in testing.
  if (!profile) {
    return;
  }
  shared_url_loader_factory_ = profile->GetDefaultStoragePartition()
                                   ->GetURLLoaderFactoryForBrowserProcess();
}

HttpRequestManager::~HttpRequestManager() = default;

void HttpRequestManager::MakeRequest(const GURL& url,
                                     const base::TimeDelta& timeout,
                                     HttpRequestCallback callback) {
  DCHECK(url.is_valid());

  auto request = std::make_unique<network::ResourceRequest>();
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->method = kGetMethod;
  request->url = url;

  simple_url_loader_ = network::SimpleURLLoader::Create(std::move(request),
                                                        GetTrafficAnnotation());
  simple_url_loader_->SetRetryOptions(
      kMaxRetries,
      network::SimpleURLLoader::RetryMode::RETRY_ON_5XX |
          network::SimpleURLLoader::RetryMode::RETRY_ON_NETWORK_CHANGE);
  simple_url_loader_->SetTimeoutDuration(timeout);
  // |simple_url_loader_| is owned by |this|, so Unretained is safe to use.
  simple_url_loader_->DownloadHeadersOnly(
      shared_url_loader_factory_.get(),
      base::BindOnce(&HttpRequestManager::OnURLLoadComplete,
                     base::Unretained(this), std::move(callback)));
}

void HttpRequestManager::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory) {
  shared_url_loader_factory_ = shared_url_loader_factory;
}

void HttpRequestManager::OnURLLoadComplete(
    HttpRequestCallback callback,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK(simple_url_loader_);
  bool connected = headers && headers->response_code() == net::HTTP_NO_CONTENT;
  simple_url_loader_.reset();
  std::move(callback).Run(connected);
}

}  // namespace network_diagnostics
}  // namespace ash
