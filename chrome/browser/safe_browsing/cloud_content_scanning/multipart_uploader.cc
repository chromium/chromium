// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_uploader.h"

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/multipart_data_pipe_getter.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/c/system/data_pipe.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

// Constants associated with exponential backoff. On each failure, we will
// increase the backoff by `kBackoffFactor`, starting from
// `kInitialBackoffSeconds`. If we fail after `kMaxRetryAttempts` retries, the
// upload fails.
const int kInitialBackoffSeconds = 1;
const int kBackoffFactor = 2;
const int kMaxRetryAttempts = 2;

// Content type of a full multipart request
const char kUploadContentType[] = "multipart/related; boundary=";

// Content type of the metadata and file contents.
const char kDataContentType[] = "Content-Type: application/octet-stream";

std::unique_ptr<MultipartDataPipeGetter> CreateFileDataPipeGetterBlocking(
    const std::string& boundary,
    const std::string& metadata,
    const base::FilePath& path) {
  // FLAG_WIN_SHARE_DELETE is necessary to allow the file to be renamed by the
  // user clicking "Open Now" without causing download errors.
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                            base::File::FLAG_WIN_SHARE_DELETE);

  return MultipartDataPipeGetter::Create(boundary, metadata, std::move(file));
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
      data_source_(STRING),
      data_(data),
      boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_source_(FILE),
      path_(path),
      boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MultipartUploadRequest::MultipartUploadRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    Callback callback)
    : base_url_(base_url),
      metadata_(metadata),
      data_source_(PAGE),
      page_region_(std::move(page_region)),
      boundary_(net::GenerateMimeMultipartBoundary()),
      callback_(std::move(callback)),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      retry_count_(0),
      url_loader_factory_(url_loader_factory),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

MultipartUploadRequest::~MultipartUploadRequest() {
  // Take ownership of the file in `data_pipe_getter_` if there is one to close
  // it on another thread since it makes blocking calls.
  if (!data_pipe_getter_)
    return;

  auto file = data_pipe_getter_->ReleaseFile();
  if (file) {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(
            [](std::unique_ptr<
                MultipartDataPipeGetter::InternalMemoryMappedFile> file) {},
            std::move(file)));
  }
}

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

  if (access_token_.empty()) {
    resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  } else {
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token_);
  }

  switch (data_source_) {
    case STRING:
      SendStringRequest(std::move(resource_request));
      break;
    case FILE:
      SendFileRequest(std::move(resource_request));
      break;
    case PAGE:
      SendPageRequest(std::move(resource_request));
      break;
    default:
      NOTREACHED();
  }
}

void MultipartUploadRequest::SendStringRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, STRING);

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  std::string request_body = GenerateRequestBody(metadata_, data_);
  url_loader_->AttachStringForUpload(request_body,
                                     kUploadContentType + boundary_);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&MultipartUploadRequest::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void MultipartUploadRequest::SendFileRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, FILE);

  if (data_pipe_getter_) {
    // When `data_pipe_getter_` is already initialized, this means a
    // request has already been made and that this call is a retry, so the pipe
    // is reset to read from the beginning of the request body.
    data_pipe_getter_->Reset();
    CompleteSendRequest(std::move(request));
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(&CreateFileDataPipeGetterBlocking, boundary_, metadata_,
                       path_),
        base::BindOnce(&MultipartUploadRequest::DataPipeCreatedCallback,
                       weak_factory_.GetWeakPtr(), std::move(request)));
  }
}

void MultipartUploadRequest::SendPageRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  DCHECK_EQ(data_source_, PAGE);

  if (data_pipe_getter_) {
    // When `data_pipe_getter_` is already initialized, this means a
    // request has already been made and that this call is a retry, so the pipe
    // is reset to read from the beginning of the request body.
    data_pipe_getter_->Reset();
    CompleteSendRequest(std::move(request));
  } else {
    DataPipeCreatedCallback(std::move(request),
                            MultipartDataPipeGetter::Create(
                                boundary_, metadata_, std::move(page_region_)));
  }
}

void MultipartUploadRequest::DataPipeCreatedCallback(
    std::unique_ptr<network::ResourceRequest> request,
    std::unique_ptr<MultipartDataPipeGetter> data_pipe_getter) {
  if (!data_pipe_getter) {
    std::move(callback_).Run(/*success=*/false, 0, "");
    return;
  }

  data_pipe_getter_ = std::move(data_pipe_getter);
  CompleteSendRequest(std::move(request));
}

void MultipartUploadRequest::CompleteSendRequest(
    std::unique_ptr<network::ResourceRequest> request) {
  mojo::PendingRemote<network::mojom::DataPipeGetter> data_pipe_getter;
  data_pipe_getter_->Clone(data_pipe_getter.InitWithNewPipeAndPassReceiver());
  request->request_body = new network::ResourceRequestBody();
  request->request_body->AppendDataPipe(std::move(data_pipe_getter));
  request->headers.SetHeader(net::HttpRequestHeaders::kContentType,
                             kUploadContentType + boundary_);

  url_loader_ =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
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
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    std::move(callback_).Run(/*success=*/true, response_code,
                             *response_body.get());
  } else {
    if (response_code < 500 || retry_count_ >= kMaxRetryAttempts) {
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
std::unique_ptr<MultipartUploadRequest>
MultipartUploadRequest::CreateStringRequest(
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

  return factory_->CreateStringRequest(url_loader_factory, base_url, metadata,
                                       data, traffic_annotation,
                                       std::move(callback));
}

// static
std::unique_ptr<MultipartUploadRequest>
MultipartUploadRequest::CreateFileRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& path,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, path, traffic_annotation,
        std::move(callback));
  }

  return factory_->CreateFileRequest(url_loader_factory, base_url, metadata,
                                     path, traffic_annotation,
                                     std::move(callback));
}

// static
std::unique_ptr<MultipartUploadRequest>
MultipartUploadRequest::CreatePageRequest(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& base_url,
    const std::string& metadata,
    base::ReadOnlySharedMemoryRegion page_region,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    MultipartUploadRequest::Callback callback) {
  if (!factory_) {
    return std::make_unique<MultipartUploadRequest>(
        url_loader_factory, base_url, metadata, std::move(page_region),
        traffic_annotation, std::move(callback));
  }

  return factory_->CreatePageRequest(url_loader_factory, base_url, metadata,
                                     std::move(page_region), traffic_annotation,
                                     std::move(callback));
}

void MultipartUploadRequest::set_access_token(const std::string& access_token) {
  access_token_ = access_token;
}

}  // namespace safe_browsing
