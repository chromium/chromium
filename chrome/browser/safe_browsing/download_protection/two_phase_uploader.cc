// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/two_phase_uploader.h"

#include <stdint.h>

#include <limits>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace {

// Header sent on initial request to start the two phase upload process.
const char kStartHeaderKey[] = "x-goog-resumable";
const char kStartHeaderValue[] = "start";

// Header returned on initial response with URL to use for the second phase.
const char kLocationHeader[] = "Location";

const char kUploadContentType[] = "application/octet-stream";

class TwoPhaseUploaderImpl : public TwoPhaseUploader {
 public:
  TwoPhaseUploaderImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      base::TaskRunner* file_task_runner,
      const GURL& base_url,
      const std::string& metadata,
      const base::FilePath& file_path,
      FinishCallback finish_callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  TwoPhaseUploaderImpl(const TwoPhaseUploaderImpl&) = delete;
  TwoPhaseUploaderImpl& operator=(const TwoPhaseUploaderImpl&) = delete;

  ~TwoPhaseUploaderImpl() override;

  // Begins the upload process.
  void Start() override;

  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

 private:
  void UploadMetadata();
  void UploadFile();
  void Finish(int net_error, int response_code, const std::string& response);

  State state_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  scoped_refptr<base::TaskRunner> file_task_runner_;
  GURL base_url_;
  GURL upload_url_;
  std::string metadata_;
  const base::FilePath file_path_;
  FinishCallback finish_callback_;
  net::NetworkTrafficAnnotationTag traffic_annotation_;

  std::unique_ptr<network::SimpleURLLoader> url_loader_;
};

TwoPhaseUploaderImpl::TwoPhaseUploaderImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TaskRunner* file_task_runner,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    FinishCallback finish_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : state_(STATE_NONE),
      url_loader_factory_(url_loader_factory),
      file_task_runner_(file_task_runner),
      base_url_(base_url),
      metadata_(metadata),
      file_path_(file_path),
      finish_callback_(std::move(finish_callback)),
      traffic_annotation_(traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

TwoPhaseUploaderImpl::~TwoPhaseUploaderImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void TwoPhaseUploaderImpl::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK_EQ(STATE_NONE, state_);

  UploadMetadata();
}

void TwoPhaseUploaderImpl::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  DVLOG(1) << __func__ << " " << url_loader_->GetFinalURL().spec() << " "
           << url_loader_->NetError() << " " << response_code;

  if (url_loader_->NetError() != net::OK) {
    LOG(ERROR) << "URLFetcher failed, err=" << url_loader_->NetError();
    Finish(url_loader_->NetError(), response_code, std::string());
    return;
  }

  switch (state_) {
    case UPLOAD_METADATA: {
      if (response_code != 201) {
        LOG(ERROR) << "Invalid response to initial request: " << response_code;
        Finish(net::OK, response_code, *response_body.get());
        return;
      }
      std::string location;
      if (!url_loader_->ResponseInfo() ||
          !url_loader_->ResponseInfo()->headers ||
          !url_loader_->ResponseInfo()->headers->EnumerateHeader(
              nullptr, kLocationHeader, &location)) {
        LOG(ERROR) << "no location header";
        Finish(net::OK, response_code, std::string());
        return;
      }
      DVLOG(1) << "upload location: " << location;
      upload_url_ = GURL(location);
      UploadFile();
      break;
    }
    case UPLOAD_FILE:
      if (response_code != 200) {
        LOG(ERROR) << "Invalid response to upload request: " << response_code;
      } else {
        state_ = STATE_SUCCESS;
      }
      Finish(net::OK, response_code, *response_body.get());
      return;
    default:
      NOTREACHED();
  }
}

void TwoPhaseUploaderImpl::UploadMetadata() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  state_ = UPLOAD_METADATA;
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = base_url_;
  resource_request->method = "POST";
  resource_request->headers.SetHeader(kStartHeaderKey, kStartHeaderValue);
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachStringForUpload(metadata_, kUploadContentType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&TwoPhaseUploaderImpl::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void TwoPhaseUploaderImpl::UploadFile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  state_ = UPLOAD_FILE;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = upload_url_;
  resource_request->method = "PUT";
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachFileForUpload(file_path_, kUploadContentType);
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&TwoPhaseUploaderImpl::OnURLLoaderComplete,
                     base::Unretained(this)));
}

void TwoPhaseUploaderImpl::Finish(int net_error,
                                  int response_code,
                                  const std::string& response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::move(finish_callback_).Run(state_, net_error, response_code, response);
}

}  // namespace

// static
TwoPhaseUploaderFactory* TwoPhaseUploader::factory_ = nullptr;

// static
std::unique_ptr<TwoPhaseUploader> TwoPhaseUploader::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::TaskRunner* file_task_runner,
    const GURL& base_url,
    const std::string& metadata,
    const base::FilePath& file_path,
    FinishCallback finish_callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!factory_) {
    return base::WrapUnique(new TwoPhaseUploaderImpl(
        url_loader_factory, file_task_runner, base_url, metadata, file_path,
        std::move(finish_callback), traffic_annotation));
  }
  return TwoPhaseUploader::factory_->CreateTwoPhaseUploader(
      url_loader_factory, file_task_runner, base_url, metadata, file_path,
      std::move(finish_callback), traffic_annotation);
}
