// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/resumable_uploader.h"

// TODO(crbug.com/456489971): Remove unused header files
#include <memory>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "components/enterprise/connectors/core/features.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

using ::enterprise_connectors::ConnectorDataPipeGetter;
using ::enterprise_connectors::ConnectorUploadRequest;

// TODO(crbug.com/456489971): Remove copies between ResumableUploadRequest and
// ResumableUploadRequestBase HTTP headers for resumable upload requests
constexpr char kUploadUrlHeader[] = "X-Goog-Upload-Url";
constexpr char kUploadIntermediateHeader[] =
    "X-Goog-Upload-Header-Cep-Response";

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
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 get_data_result,
                                 path,
                                 file_size,
                                 is_obfuscated,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload) {
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
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 get_data_result,
                                 std::move(page_region),
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload) {
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
    : ResumableUploadRequestBase(std::move(url_loader_factory),
                                 base_url,
                                 metadata,
                                 data,
                                 histogram_suffix,
                                 traffic_annotation,
                                 std::move(verdict_received_callback),
                                 std::move(content_uploaded_callback),
                                 force_sync_upload) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

ResumableUploadRequest::~ResumableUploadRequest() = default;

void ResumableUploadRequest::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SendMetadataRequest();
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

}  // namespace safe_browsing
