// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/thread_pool.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

// HTTP headers for resumable upload requests
constexpr char kUploadProtocolHeader[] = "X-Goog-Upload-Protocol";
constexpr char kUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kUploadHeaderContentLengthHeader[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kUploadHeaderContentTypeHeader[] =
    "X-Goog-Upload-Header-Content-Type";
// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";

}  // namespace

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             path,
                             file_size,
                             traffic_annotation,
                             std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             traffic_annotation,
                             std::move(callback)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::~ResumableUploadRequest() = default;

void ResumableUploadRequest::SetMetadataRequestHeaders(
    network::ResourceRequest* request) {
  CHECK(request);
  // Both page and file requests should have non-zero `data_size_`.
  DCHECK_GT(data_size_, (uint64_t)0);

  request->headers.SetHeader(kUploadProtocolHeader, "resumable");
  request->headers.SetHeader(kUploadCommandHeader, "start");
  request->headers.SetHeader(kUploadHeaderContentLengthHeader,
                             base::NumberToString(data_size_));
  request->headers.SetHeader(kUploadHeaderContentTypeHeader,
                             kUploadContentType);

  if (access_token_.empty()) {
    request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  } else {
    SetAccessTokenAndClearCookieInResourceRequest(request, access_token_);
  }
}

// static
ResumableUploadRequestFactory* ResumableUploadRequest::factory_ = nullptr;

// static
std::unique_ptr<ResumableUploadRequest>
ResumableUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    uint64_t file_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ResumableUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<ResumableUploadRequest>(
        url_loader_factory, base_url, metadata, path, file_size,
        traffic_annotation, std::move(callback));
  }

  return factory_->CreateFileRequest(url_loader_factory, base_url, metadata,
                                     path, file_size, traffic_annotation,
                                     std::move(callback));
}

// static
std::unique_ptr<ResumableUploadRequest>
ResumableUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ResumableUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<ResumableUploadRequest>(
        url_loader_factory, base_url, metadata, std::move(page_region),
        traffic_annotation, std::move(callback));
  }

  return factory_->CreatePageRequest(url_loader_factory, base_url, metadata,
                                     std::move(page_region), traffic_annotation,
                                     std::move(callback));
}

void ResumableUploadRequest::SendMetadataRequest() {
  // TODO(b/322006084): 1. Create metadata request headers 2. Attach content
  // type and metadata to the request.
}

}  // namespace safe_browsing
