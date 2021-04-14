// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace safe_browsing {

namespace {

// Constants associated with exponential backoff. On each failure, we will
// increase the backoff by |kBackoffFactor|, starting from
// |kInitialBackoffSeconds|. If we fail after |kMaxRetryAttempts| retries, the
// upload fails.
const int kInitialBackoffSeconds = 1;
const int kBackoffFactor = 2;
const int kMaxRetryAttempts = 2;

// Content type of a full multipart request
const char kUploadContentType[] = "multipart/related; boundary=";

// Content type of the metadata and file contents.
const char kDataContentType[] = "Content-Type: application/octet-stream";

void RecordUploadSuccessHistogram(bool success) {
  base::UmaHistogramBoolean("SBMultipartUploader.UploadSuccess", success);
}

}  // namespace

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_(data),
      boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)),
      current_backoff_(base::TimeDelta::FromSeconds(kInitialBackoffSeconds)),
      retry_count_(0),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MultipartUploadRequest::~MultipartUploadRequest() {}

void MultipartUploadRequest::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  start_time_ = base::Time::Now();
  SendRequest();
}

std::string MultipartUploadRequest::GenerateRequestBody(
    const std::string& metadata,
    const std::string& data) {
  return base::StrCat({"--", boundary_, "\r\n", kDataContentType, "\r\n\r\n",
                       metadata, "\r\n--", boundary_, "\r\n", kDataContentType,
                       "\r\n\r\n", data, "\r\n--", boundary_, "--\r\n"});
}

void MultipartUploadRequest::SendRequest() {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  resource_request->headers.SetHeader("X-Goog-Upload-Protocol", "multipart");

  if (base::FeatureList::IsEnabled(kSafeBrowsingRemoveCookies)) {
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  }

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);

  std::string request_body = GenerateRequestBody(metadata_, data_);
  base::UmaHistogramMemoryKB("SBMultipartUploader.UploadSize",
                             request_body.size());
  url_loader_->AttachStringForUpload(request_body,
                                     kUploadContentType + boundary_);

  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&MultipartUploadRequest::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void MultipartUploadRequest::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  RetryOrFinish(url_loader_->NetError(), response_code,
                std::move(response_body));
}

void MultipartUploadRequest::RetryOrFinish(
    int net_error,
    int response_code,
    std::unique_ptr<std::string> response_body) {
  base::UmaHistogramSparse(
      "SBMultipartUploader.NetworkRequestResponseCodeOrError",
      net_error == net::OK ? response_code : net_error);

  if (net_error == net::OK && response_code == net::HTTP_OK) {
    RecordUploadSuccessHistogram(/*success=*/true);
    base::UmaHistogramExactLinear("SBMultipartUploader.RetriesNeeded",
                                  retry_count_, kMaxRetryAttempts);
    base::UmaHistogramMediumTimes(
        "SBMultipartUploader.SuccessfulUploadDuration",
        base::Time::Now() - start_time_);
    std::move(callback_).Run(/*success=*/true, response_code,
                             *response_body.get());
  } else {
    if (response_code < 500 || retry_count_ >= kMaxRetryAttempts) {
      RecordUploadSuccessHistogram(/*success=*/false);
      base::UmaHistogramMediumTimes("SBMultipartUploader.FailedUploadDuration",
                                    base::Time::Now() - start_time_);
      std::move(callback_).Run(/*success=*/false, response_code,
                               *response_body.get());
    } else {
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&MultipartUploadRequest::SendRequest,
                         weak_factory_.GetWeakPtr()),
          current_backoff_);
      current_backoff_ *= kBackoffFactor;
      retry_count_++;
    }
  }
}

// static
MultipartUploadRequestFactory* MultipartUploadRequest::factory_ = nullptr;

// static
std::unique_ptr<MultipartUploadRequest> MultipartUploadRequest::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const std::string& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, data, traffic_annotation,
        std::move(callback));
  }

  return factory_->Create(url_loader_factory, base_url, metadata, data,
                          traffic_annotation, std::move(callback));
}

}  // namespace safe_browsing
