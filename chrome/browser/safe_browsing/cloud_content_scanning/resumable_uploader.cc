// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

#include <memory>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

using ::enterprise_connectors::ConnectorDataPipeGetter;
using ::enterprise_connectors::ConnectorUploadRequest;

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
constexpr char kUploadIntermediateHeader[] =
    "X-Goog-Upload-Header-Cep-Response";
// Content type of the upload contents.
constexpr char kUploadContentType[] = "application/octet-stream";
// Content type of metadata.
constexpr char kMetadataContentType[] = "application/json";
// Content type of pasted images.
constexpr char kImageContentType[] = "image/png";

std::unique_ptr<ConnectorDataPipeGetter> CreateFileDataPipeGetterBlocking(
    const base::FilePath& path,
    bool is_obfuscated) {
  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  return ConnectorDataPipeGetter::CreateResumablePipeGetter(std::move(file),
                                                            is_obfuscated);
}

bool IsSuccess(int net_error, int response_code) {
  return net_error == net::OK && response_code == net::HTTP_OK;
}

}  // namespace

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             path,
                             file_size,
                             is_obfuscated,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      get_data_result_(get_data_result),
      is_obfuscated_(is_obfuscated),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      force_sync_upload_(force_sync_upload) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             std::move(page_region),
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      get_data_result_(get_data_result),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      force_sync_upload_(force_sync_upload) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::ResumableUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload)
    : ConnectorUploadRequest(std::move(url_loader_factory),
                             base_url,
                             metadata,
                             data,
                             histogram_suffix,
                             traffic_annotation,
                             base::DoNothing()),
      verdict_received_callback_(std::move(verdict_received_callback)),
      get_data_result_(enterprise_connectors::ScanRequestUploadResult::SUCCESS),
      content_uploaded_callback_(std::move(content_uploaded_callback)),
      force_sync_upload_(force_sync_upload) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::~ResumableUploadRequest() = default;

void ResumableUploadRequest::SetMetadataRequestHeaders(
    network::ResourceRequest* request) {
  CHECK(request);

  // Page, string and file requests should have non-zero `data_size_`.
  DCHECK_GT(data_size_, (uint64_t)0);

  request->headers.SetHeader(kUploadProtocolHeader, "resumable");
  request->headers.SetHeader(kUploadCommandHeader, "start");
  request->headers.SetHeader(kUploadHeaderContentLengthHeader,
                             base::NumberToString(data_size_));
  // `STRING` is only used for resumable requests for image pasting.
  request->headers.SetHeader(
      kUploadHeaderContentTypeHeader,
      data_source_ == STRING ? kImageContentType : kUploadContentType);
  if (!access_token_.empty()) {
    LogAuthenticatedCookieResets(
        *request, SafeBrowsingAuthenticatedEndpoint::kDeepScanning);
    SetAccessToken(request, access_token_);
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
    case ASYNC:
      scan_info = "Async content upload";
      break;
  }

  return base::StrCat({"Resumable - ", scan_info});
}

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreateStringRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreateStringRequest(
        url_loader_factory, base_url, metadata, data, histogram_suffix,
        traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, data, histogram_suffix,
      traffic_annotation, std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
}

std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    const base::FilePath& path,
    uint64_t file_size,
    bool is_obfuscated,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreateFileRequest(
        url_loader_factory, base_url, metadata, get_data_result, path,
        file_size, is_obfuscated, histogram_suffix, traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, get_data_result, path, file_size,
      is_obfuscated, histogram_suffix, traffic_annotation,
      std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
}

// static
std::unique_ptr<ConnectorUploadRequest>
ResumableUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    enterprise_connectors::ScanRequestUploadResult get_data_result,
    base::ReadOnlySharedMemoryRegion page_region,
    const std::string& histogram_suffix,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    VerdictReceivedCallback verdict_received_callback,
    ContentUploadedCallback content_uploaded_callback,
    bool force_sync_upload) {
  if (factory_) {
    return factory_->CreatePageRequest(
        url_loader_factory, base_url, metadata, get_data_result,
        std::move(page_region), histogram_suffix, traffic_annotation,
        std::move(verdict_received_callback)
            .Then(std::move(content_uploaded_callback)));
  }
  return std::make_unique<ResumableUploadRequest>(
      url_loader_factory, base_url, metadata, get_data_result,
      std::move(page_region), histogram_suffix, traffic_annotation,
      std::move(verdict_received_callback),
      std::move(content_uploaded_callback), force_sync_upload);
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

  auto headers = url_loader_->ResponseInfo()->headers;
  // If there is an error or if no content upload is required,
  // CanUploadContent() returns false.
  response_code = headers->response_code();
  if (!CanUploadContent(headers)) {
    Finish(url_loader_->NetError(), response_code, std::move(response_body));
    return;
  }

  if (!force_sync_upload_) {
    if (headers->HasHeader(kUploadIntermediateHeader)) {
      response_body = headers->GetNormalizedHeader(kUploadIntermediateHeader);

      std::string output;
      bool is_decoded = base::Base64Decode(response_body.value(), &output);

      if (output.empty() || !is_decoded) {
        Finish(net::ERR_FAILED, net::HTTP_BAD_REQUEST, std::nullopt);
        return;
      }

      scan_type_ = ASYNC;
      std::move(verdict_received_callback_)
          .Run(IsSuccess(url_loader_->NetError(), response_code), response_code,
               output);
    }
  }

  // If chrome is being told to upload the content but the content is too large
  // or is encrypted and encrypted file upload is not enabled, fail now.
  if (get_data_result_ ==
          enterprise_connectors::ScanRequestUploadResult::FILE_TOO_LARGE ||
      (get_data_result_ ==
           enterprise_connectors::ScanRequestUploadResult::FILE_ENCRYPTED &&
       !ShouldUploadEncryptedFile())) {
    Finish(net::ERR_FAILED, net::HTTP_BAD_REQUEST, std::move(response_body));
    return;
  }

  // At this point, we are guaranteed to have the upload url header
  SendContentSoon(headers->GetNormalizedHeader(kUploadUrlHeader).value());
}

bool ResumableUploadRequest::ShouldUploadEncryptedFile() {
  return base::FeatureList::IsEnabled(
             enterprise_connectors::kEnableEncryptedFileUpload) &&
         scan_type_ == ASYNC;
}
void ResumableUploadRequest::SendContentSoon(const std::string& upload_url) {
  auto request = std::make_unique<network::ResourceRequest>();
  request->method = "POST";
  request->url = GURL(upload_url);
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
    // Resumable uploads are used for pasted images, which are handled as string
    // data. Using resumable uploads for pasted images is enabled by the
    // `enterprise_connectors::kDlpScanPastedImages` feature flag. Text pastes
    // use multipart uploads.
    case STRING:
      SendContentNow(std::move(request));
      break;
    default:
      NOTREACHED();
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
      base::BindOnce(&CreateFileDataPipeGetterBlocking, path_, is_obfuscated_),
      base::BindOnce(&ResumableUploadRequest::OnDataPipeCreated,
                     weak_factory_.GetWeakPtr(), std::move(request)));
}

void ResumableUploadRequest::OnDataPipeCreated(
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<ConnectorDataPipeGetter> data_pipe_getter) {
  scoped_file_access_.reset();
  if (!data_pipe_getter) {
    // TODO(329293309): Replace with meaningful net_error value since 0 does not
    // indicate an error.
    Finish(0, 0, std::nullopt);
    return;
  }

  data_pipe_getter_ = std::move(data_pipe_getter);
  SendContentNow(std::move(request));
}

void ResumableUploadRequest::SendContentNow(
    std::unique_ptr<network::ResourceRequest> request) {
  // `data_pipe_getter_` is null for STRING requests, which are handled by
  // attaching the string data directly to the URL loader. For FILE and PAGE
  // requests, `data_pipe_getter_` will be non-null.
  if (data_pipe_getter_) {
    mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
    data_pipe_getter_->Clone(data_pipe_getter.InitWithNewPipeAndPassReceiver());
    request->request_body = new network::ResourceRequestBody();
    request->request_body->AppendDataPipe(std::move(data_pipe_getter));
  }

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);

  if (!data_pipe_getter_) {
    url_loader_->AttachStringForUpload(data_, kImageContentType);
  }

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResumableUploadRequest::OnSendContentCompleted,
                     weak_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ResumableUploadRequest::OnSendContentCompleted(
    base::TimeTicks start_time,
    std::optional<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If this has already been called after the metadata check, that means that
  // we have set the value to ASYNC.
  if (!verdict_received_callback_.is_null()) {
    scan_type_ = FULL_CONTENT;
  }

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
  std::optional<std::string> upload_status =
      headers->GetNormalizedHeader(kUploadStatusHeader);
  if (!upload_status || !headers->HasHeader(kUploadUrlHeader)) {
    return false;
  }
  return base::EqualsCaseInsensitiveASCII(upload_status.value_or(std::string()),
                                          "active");
}

void ResumableUploadRequest::Finish(int net_error,
                                    int response_code,
                                    std::optional<std::string> response_body) {
  if (!histogram_suffix_.empty()) {
    std::string histogram = base::StrCat(
        {"SafeBrowsing.ResumableUploader.NetworkResult.", histogram_suffix_});
    RecordHttpResponseOrErrorCode(histogram.c_str(), net_error, response_code);
  }

  // The callback may have been invoked when the metadata verdict was received
  // with the CEP header, to unblock the user initiate an async upload.
  if (!verdict_received_callback_.is_null()) {
    std::move(verdict_received_callback_)
        .Run(/*success=*/IsSuccess(net_error, response_code), response_code,
             response_body.value_or(""));
  }
  std::move(content_uploaded_callback_).Run();
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
