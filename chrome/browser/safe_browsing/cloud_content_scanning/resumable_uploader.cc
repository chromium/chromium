// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/file_access/scoped_file_access_delegate.h"
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
constexpr char kUploadOffsetHeader[] = "X-Goog-Upload-Offset";
// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";
// Content type of metadata.
constexpr char kMetadataContentType[] = "application/json";

std::unique_ptr<ConnectorDataPipeGetter> CreateFileDataPipeGetterBlocking(
    const base::FilePath& path) {
  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  return ConnectorDataPipeGetter::CreateResumablePipeGetter(std::move(file));
}

}  // namespace

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
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
                             std::move(callback)),
      get_data_result_(get_data_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             traffic_annotation,
                             std::move(callback)),
      get_data_result_(get_data_result) {
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

  if (!access_token_.empty()) {
    LogAuthenticatedCookieResets(
        *request, SafeBrowsingAuthenticatedEndpoint::kDeepScanning);
    SetAccessTokenAndClearCookieInResourceRequest(request, access_token_);
  }
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
}

void ResumableUploadRequest::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SendMetadataRequest();
}

std::string ResumableUploadRequest::GetUploadInfo() {
  std::string scan_info;
  switch (scan_type_) {
    case PENDING:
      scan_info = "Pending";
      break;
    case FULL_CONTENT:
      scan_info = "Full content scan";
      break;
    case METADATA_ONLY:
      scan_info = "Metadata only scan";
      break;
  }

  return base::StrCat({"Resumable - ", scan_info});
}

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ResumableUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<ResumableUploadRequest>(
        url_loader_factory, base_url, metadata, get_data_result, path,
        file_size, traffic_annotation, std::move(callback));
  }

  return factory_->CreateFileRequest(url_loader_factory, base_url, metadata,
                                     get_data_result, path, file_size,
                                     traffic_annotation, std::move(callback));
}

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    BinaryUploadService::Result get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    ResumableUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<ResumableUploadRequest>(
        url_loader_factory, base_url, metadata, get_data_result,
        std::move(page_region), traffic_annotation, std::move(callback));
  }

  return factory_->CreatePageRequest(url_loader_factory, base_url, metadata,
                                     get_data_result, std::move(page_region),
                                     traffic_annotation, std::move(callback));
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
      base::BindOnce(&ResumableUploadRequest::OnMetadataUploadCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ResumableUploadRequest::OnMetadataUploadCompleted(
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scan_type_ = METADATA_ONLY;

  base::UmaHistogramCustomTimes(
      base::StrCat({"Enterprise.ResumableRequest.MetadataCheck.",
                    GetRequestType(), ".Duration"}),
      base::TimeTicks::Now() - start_time, base::Milliseconds(1),
      base::Minutes(6), 50);

  int response_code = 0;
  if (!url_loader_->ResponseInfo() || !url_loader_->ResponseInfo()->headers) {
    // TODO(b/322005992): Add retry logics.
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  // If there is an error or if the metadata check has already determined a
  // verdict, CanUploadContent() returns false.
  response_code = url_loader_->ResponseInfo()->headers->response_code();
  if (!CanUploadContent(url_loader_->ResponseInfo()->headers)) {
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  // If chrome is being told to upload the content but the content is too large
  // or is encrypted, fail now.
  if (get_data_result_ == BinaryUploadService::Result::FILE_TOO_LARGE ||
      get_data_result_ == BinaryUploadService::Result::FILE_ENCRYPTED) {
    Finish(net::ERR_FAILED, net::HTTP_BAD_REQUEST, std::move(response_body));
    return;
  }

  SendContentSoon();
}

void ResumableUploadRequest::SendContentSoon() {
  auto request = std::make_unique<network::ResourceRequest>();
  request->method = "POST";
  request->url = GURL(upload_url_);
  // Only sends content smaller than 50MB, in a single request.
  request->headers.SetHeader(kUploadCommandHeader, "upload, finalize");
  request->headers.SetHeader(kUploadOffsetHeader, "0");

  // TODO(b/322005992): Add retry logics.
  switch (data_source_) {
    case FILE:
      file_access::RequestFilesAccessForSystem(
          {path_},
          base::BindOnce(&ResumableUploadRequest::CreateDatapipe,
                         weak_factory_.GetWeakPtr(), std::move(request)));
      break;
    case PAGE:
      OnDataPipeCreated(std::move(request),
                        ConnectorDataPipeGetter::CreateResumablePipeGetter(
                            std::move(page_region_)));
      break;
    // Resumable upload currently does not support paste.
    case STRING:
      NOTREACHED_IN_MIGRATION();
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }
}

// TODO(b/328415950): Move the data pipe creation logics to
// connector_upload_request.
void ResumableUploadRequest::CreateDatapipe(
    std::unique_ptr<network::ResourceRequest> request,
    file_access::ScopedFileAccess file_access) {
  scoped_file_access_ =
      std::make_unique<file_access::ScopedFileAccess>(std::move(file_access));
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CreateFileDataPipeGetterBlocking, path_),
      base::BindOnce(&ResumableUploadRequest::OnDataPipeCreated,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ResumableUploadRequest::OnDataPipeCreated(
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter) {
  scoped_file_access_.reset();
  if (!data_pipe_getter) {
    std::move(callback_).Run(/*success=*/false, 0, "");
    return;
  }

  data_pipe_getter_ = std::move(data_pipe_getter);
  SendContentNow(std::move(request));
}

void ResumableUploadRequest::SendContentNow(
    std::unique_ptr<network::ResourceRequest> request) {
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
  data_pipe_getter_->Clone(data_pipe_getter.InitWithNewPipeAndPassReceiver());
  request->request_body = new network::ResourceRequestBody();
  request->request_body->AppendDataPipe(std::move(data_pipe_getter));

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequest::OnSendContentCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ResumableUploadRequest::OnSendContentCompleted(
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  scan_type_ = FULL_CONTENT;

  base::UmaHistogramCustomTimes(
      base::StrCat({"Enterprise.ResumableRequest.ContentCheck.",
                    GetRequestType(), ".Duration"}),
      base::TimeTicks::Now() - start_time, base::Milliseconds(1),
      base::Minutes(6), 50);

  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers) {
    response_code = url_loader_->ResponseInfo()->headers->response_code();
  }

  Finish(url_loader_->NetError(), response_code, std::move(response_body));
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

std::string ResumableUploadRequest::GetRequestType() {
  switch (data_source_) {
    case FILE:
      return "File";
    case STRING:
      return "Text";
    case PAGE:
      return "Print";
  }
}

}  // namespace safe_browsing
