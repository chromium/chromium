// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

// HTTP headers for resumable upload requests
constexpr char kUploadProtocolHeader[] = "X-Goog-Upload-Protocol";
constexpr char kUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kUploadHeaderContentLengthHeader[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kUploadHeaderContentTypeHeader[] =
    "X-Goog-Upload-Header-Content-Type";
constexpr char kUploadStatusHeader[] = "X-Goog-Upload-Status";
constexpr char kUploadUrlHeader[] = "X-Goog-Upload-Url";
// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";
// Content type of metadata.
constexpr char kMetadataContentType[] = "application/json";
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
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  SetMetadataRequestHeaders(resource_request.get());

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachStringForUpload(base::StrCat({metadata_, "\r\n"}),
                                     kMetadataContentType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequest::OnMetadataUploadComplete,
                     weak_factory_.GetWeakPtr()));
}

void ResumableUploadRequest::OnMetadataUploadComplete(
    std::optional<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int response_code = 0;
  if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
    // TODO(b/322005992): Add retry logics.
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  response_code = url_loader_->ResponseInfo()->headers->response_code();
  if (!CanUploadContent(url_loader_->ResponseInfo()->headers)) {
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  // TODO(b/322005479): Add SendContent() logics.
}

bool ResumableUploadRequest::CanUploadContent(
    const scoped_refptr<net::HttpResponseHeaders>& headers) {
  if (headers->response_code() != net::HTTP_OK) {
    return false;
  }
  std::string upload_status;
  if (!headers->GetNormalizedHeader(kUploadStatusHeader, &upload_status) ||
      !headers->GetNormalizedHeader(kUploadUrlHeader, &upload_url_)) {
    return false;
  }
  return base::EqualsCaseInsensitiveASCII(upload_status, "active");
}

void ResumableUploadRequest::Finish(int net_error,
                                    int response_code,
                                    std::optional<std::string> response_body) {
  // TODO(b/322005992): Add retry logics and consider sharing them with
  // MultipartUploadRequest.
  std::move(callback_).Run(
      /*success=*/net_error == net::OK && response_code == net::HTTP_OK,
      response_code, response_body.value_or(""));
}

}  // namespace safe_browsing
