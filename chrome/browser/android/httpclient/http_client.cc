// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/android/httpclient/http_client.h"

#include <string>
#include <utility>

#include "base/not_fatal_until.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace httpclient {

namespace {
constexpr int kTimeoutDurationSeconds = 30;
constexpr size_t kMaxResponseSizeDefault = 4 * 1024 * 1024;

void PopulateRequestBodyAndContentType(network::SimpleURLLoader* loader,
                                       std::vector<uint8_t>&& request_body,
                                       const std::string& content_type) {
  std::string request_body_string(
      reinterpret_cast<const char*>(request_body.data()), request_body.size());

  loader->AttachStringForUpload(request_body_string, content_type);
}

std::unique_ptr<network::SimpleURLLoader> MakeLoader(
    const GURL& gurl,
    const std::string& request_type,
    std::vector<uint8_t>&& request_body,
    std::map<std::string, std::string>&& headers,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = gurl;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = request_type;

  std::string content_type;
  for (auto const& [key, value] : headers) {
    if (0 == base::CompareCaseInsensitiveASCII(
                 key, net::HttpRequestHeaders::kContentType)) {
      // Content-Type will be populated in ::PopulateRequestBodyAndContentType.
      content_type = value;
      continue;
    }
    resource_request->headers.SetHeader(key, value);
  }

  auto simple_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), network_traffic_annotation);
  simple_loader->SetTimeoutDuration(base::Seconds(kTimeoutDurationSeconds));

  if (!request_body.empty()) {
    DCHECK(!content_type.empty());
    PopulateRequestBodyAndContentType(simple_loader.get(),
                                      std::move(request_body), content_type);
  }

  return simple_loader;
}
}  // namespace

HttpClient::HttpClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : loader_factory_(url_loader_factory) {}

HttpClient::~HttpClient() {
  url_loaders_.clear();
}

void HttpClient::Send(
    const GURL& gurl,
    const std::string& request_type,
    std::vector<uint8_t>&& request_body,
    std::map<std::string, std::string>&& headers,
    const net::NetworkTrafficAnnotationTag& network_traffic_annotation,
    HttpClient::ResponseCallback callback) {
  // SimpleUrlLoader can only be called on the UI thread.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  std::unique_ptr<network::SimpleURLLoader> simple_loader =
      MakeLoader(gurl, request_type, std::move(request_body),
                 std::move(headers), network_traffic_annotation);
  simple_loader->SetAllowHttpErrorResults(true);
  // TODO(crbug.com/40169299): Use flag to control the max size limit.
  simple_loader->DownloadToString(
      loader_factory_.get(),
      base::BindOnce(&HttpClient::OnSimpleLoaderComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     simple_loader.get()),
      kMaxResponseSizeDefault);
  // We need to keep loaders alive, so we store a reference.
  url_loaders_.emplace(std::move(simple_loader));
}

void HttpClient::ReleaseUrlLoader(network::SimpleURLLoader* simple_loader) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Release the current loader.
  auto loader_iter = url_loaders_.find(simple_loader);
  CHECK(loader_iter != url_loaders_.end(), base::NotFatalUntil::M130);
  url_loaders_.erase(loader_iter);
}

void HttpClient::OnSimpleLoaderComplete(
    HttpClient::ResponseCallback response_callback,
    network::SimpleURLLoader* simple_loader,
    std::unique_ptr<std::string> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int32_t response_code = 0;
  int32_t net_error_code = simple_loader->NetError();

  std::map<std::string, std::string> response_headers;
  auto* response_info = simple_loader->ResponseInfo();
  if (response_info && response_info->headers) {
    response_code = response_info->headers->response_code();

    size_t iter = 0;
    std::string name, value;
    while (response_info->headers->EnumerateHeaderLines(&iter, &name, &value)) {
      std::string& slot = response_headers[name];
      if (slot.empty()) {
        slot = std::move(value);
      } else {
        slot += '\n';
        slot += value;
      }
    }
  }

  // If the response string is empty, that means a network error exists.
  // We'll not populate the response body in that case.
  std::vector<uint8_t> response_body;
  if (response) {
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(response->data());
    const uint8_t* end = begin + response->size();
    response_body.assign(begin, end);
  }

  ReleaseUrlLoader(simple_loader);

  std::move(response_callback)
      .Run(response_code, net_error_code, std::move(response_body),
           std::move(response_headers));
}

}  // namespace httpclient
