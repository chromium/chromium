// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/uploading/upload_job_impl.h"

#include <stddef.h>

#include <memory>
#include <set>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/stringprintf.h"
#include "base/syslog_logging.h"
#include "base/task/sequenced_task_runner.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/mime_util.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace policy {

namespace {

// Format for bearer tokens in HTTP requests to access OAuth 2.0 protected
// resources.
const char kAuthorizationHeaderFormat[] = "Bearer %s";

// Value the "Content-Type" field will be set to in the POST request.
const char kUploadContentType[] = "multipart/form-data";

// Number of upload attempts.
const int kMaxAttempts = 4;

// Max size of MIME boundary according to RFC 1341, section 7.2.1.
const size_t kMaxMimeBoundarySize = 70;

// Delay after each unsuccessful upload attempt.
long g_retry_delay_ms = 25000;

}  // namespace

UploadJobImpl::Delegate::~Delegate() {}

UploadJobImpl::MimeBoundaryGenerator::~MimeBoundaryGenerator() {}

UploadJobImpl::RandomMimeBoundaryGenerator::~RandomMimeBoundaryGenerator() {}

// multipart/form-data POST request to upload the data. A DataSegment
// corresponds to one "Content-Disposition" in the "multipart" request.
class DataSegment {
 public:
  DataSegment(const std::string& name,
              const std::string& filename,
              std::unique_ptr<std::string> data,
              const std::map<std::string, std::string>& header_entries);

  DataSegment(const DataSegment&) = delete;
  DataSegment& operator=(const DataSegment&) = delete;

  // Returns the header entries for this DataSegment.
  const std::map<std::string, std::string>& GetHeaderEntries() const;

  // Returns the string that will be assigned to the |name| field in the header.
  // |name| must be unique throughout the multipart message. This is enforced in
  // SetUpMultipart().
  const std::string& GetName() const;

  // Returns the string that will be assigned to the |filename| field in the
  // header. If the |filename| is the empty string, the header field will be
  // omitted.
  const std::string& GetFilename() const;

  // Returns the data contained in this DataSegment. Ownership is passed.
  std::unique_ptr<std::string> GetData();

  // Returns the size in bytes of the blob in |data_|.
  size_t GetDataSize() const;

 private:
  const std::string name_;
  const std::string filename_;
  std::unique_ptr<std::string> data_;
  std::map<std::string, std::string> header_entries_;
};

DataSegment::DataSegment(
    const std::string& name,
    const std::string& filename,
    std::unique_ptr<std::string> data,
    const std::map<std::string, std::string>& header_entries)
    : name_(name),
      filename_(filename),
      data_(std::move(data)),
      header_entries_(header_entries) {
  DCHECK(data_);
}

const std::map<std::string, std::string>& DataSegment::GetHeaderEntries()
    const {
  return header_entries_;
}

const std::string& DataSegment::GetName() const {
  return name_;
}

const std::string& DataSegment::GetFilename() const {
  return filename_;
}

std::unique_ptr<std::string> DataSegment::GetData() {
  return std::move(data_);
}

size_t DataSegment::GetDataSize() const {
  DCHECK(data_);
  return data_->size();
}

std::string UploadJobImpl::RandomMimeBoundaryGenerator::GenerateBoundary()
    const {
  return net::GenerateMimeMultipartBoundary();
}

UploadJobImpl::UploadJobImpl(
    const GURL& upload_url,
    const CoreAccountId& account_id,
    OAuth2AccessTokenManager* access_token_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    Delegate* delegate,
    std::unique_ptr<MimeBoundaryGenerator> boundary_generator,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : OAuth2AccessTokenManager::Consumer("cros_upload_job"),
      upload_url_(upload_url),
      account_id_(account_id),
      access_token_manager_(access_token_manager),
      url_loader_factory_(std::move(url_loader_factory)),
      delegate_(delegate),
      boundary_generator_(std::move(boundary_generator)),
      traffic_annotation_(traffic_annotation),
      state_(IDLE),
      retry_(0),
      task_runner_(task_runner) {
  DCHECK(access_token_manager_);
  DCHECK(url_loader_factory_);
  DCHECK(delegate_);
  SYSLOG(INFO) << "Upload job created.";
  if (!upload_url_.is_valid()) {
    state_ = ERROR;
    NOTREACHED_IN_MIGRATION() << upload_url_ << " is not a valid URL.";
  }
}

UploadJobImpl::~UploadJobImpl() {
  if (state_ != ERROR && state_ != SUCCESS) {
    SYSLOG(ERROR) << "Upload job interrupted.";
  }
}

void UploadJobImpl::AddDataSegment(
    const std::string& name,
    const std::string& filename,
    const std::map<std::string, std::string>& header_entries,
    std::unique_ptr<std::string> data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Cannot add data to busy or failed instance.
  DCHECK_EQ(IDLE, state_);
  if (state_ != IDLE)
    return;

  data_segments_.push_back(std::make_unique<DataSegment>(
      name, filename, std::move(data), header_entries));
}

void UploadJobImpl::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Cannot start an upload on a busy or failed instance.
  DCHECK_EQ(IDLE, state_);
  if (state_ != IDLE)
    return;
  DCHECK_EQ(0, retry_);

  SYSLOG(INFO) << "Upload job started";
  RequestAccessToken();
}

// static
void UploadJobImpl::SetRetryDelayForTesting(long retry_delay_ms) {
  CHECK_GE(retry_delay_ms, 0);
  g_retry_delay_ms = retry_delay_ms;
}

void UploadJobImpl::RequestAccessToken() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!access_token_request_);
  SYSLOG(INFO) << "Requesting access token.";

  state_ = ACQUIRING_TOKEN;

  OAuth2AccessTokenManager::ScopeSet scope_set;
  scope_set.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  access_token_request_ =
      access_token_manager_->StartRequest(account_id_, scope_set, this);
}

bool UploadJobImpl::SetUpMultipart() {
  DCHECK_EQ(ACQUIRING_TOKEN, state_);
  state_ = PREPARING_CONTENT;

  if (mime_boundary_ && post_data_)
    return true;

  std::set<std::string> used_names;

  // Check uniqueness of header field names.
  for (const auto& data_segment : data_segments_) {
    if (!used_names.insert(data_segment->GetName()).second)
      return false;
  }

  mime_boundary_ =
      std::make_unique<std::string>(boundary_generator_->GenerateBoundary());

  // Estimate an upper bound for the total message size to make memory
  // allocation more efficient. It is not an error if this turns out to be too
  // small as std::string will take care of the realloc.
  size_t size = 0;
  for (const auto& data_segment : data_segments_) {
    for (const auto& entry : data_segment->GetHeaderEntries())
      size += entry.first.size() + entry.second.size();
    size += kMaxMimeBoundarySize + data_segment->GetName().size() +
            data_segment->GetFilename().size() + data_segment->GetDataSize();
    // Add some extra space for all the constants and control characters.
    size += 128;
  }

  // Allocate memory of the expected size.
  post_data_ = std::make_unique<std::string>();
  post_data_->reserve(size);

  for (const auto& data_segment : data_segments_) {
    post_data_->append("--" + *mime_boundary_.get() + "\r\n");
    post_data_->append("Content-Disposition: form-data; name=\"" +
                       data_segment->GetName() + "\"");
    if (!data_segment->GetFilename().empty()) {
      post_data_->append("; filename=\"" + data_segment->GetFilename() + "\"");
    }
    post_data_->append("\r\n");

    // Add custom header fields.
    for (const auto& entry : data_segment->GetHeaderEntries()) {
      post_data_->append(entry.first + ": " + entry.second + "\r\n");
    }
    std::unique_ptr<std::string> data = data_segment->GetData();
    post_data_->append("\r\n" + *data + "\r\n");
  }
  post_data_->append("--" + *mime_boundary_.get() + "--\r\n");

  // Issues a warning if our buffer size estimate was too small.
  if (post_data_->size() > size) {
    SYSLOG(INFO)
        << "Reallocation needed in POST data buffer. Expected maximum size "
        << size << " bytes, actual size " << post_data_->size() << " bytes.";
  }

  // Discard the data segments as they are not needed anymore from here on.
  data_segments_.clear();

  return true;
}

void UploadJobImpl::CreateAndStartURLLoader(const std::string& access_token) {
  // Ensure that the content has been prepared and the upload url is valid.
  DCHECK_EQ(PREPARING_CONTENT, state_);
  SYSLOG(INFO) << "Starting URL fetcher.";

  std::string content_type = kUploadContentType;
  content_type.append("; boundary=");
  content_type.append(*mime_boundary_.get());

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = upload_url_;
  resource_request->headers.SetHeader(
      net::HttpRequestHeaders::kAuthorization,
      base::StringPrintf(kAuthorizationHeaderFormat, access_token.c_str()));

  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation_);
  url_loader_->AttachStringForUpload(*post_data_, content_type);
  url_loader_->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindOnce(&UploadJobImpl::OnURLLoadComplete,
                     base::Unretained(this)));
}

void UploadJobImpl::StartUpload() {
  DCHECK(thread_checker_.CalledOnValidThread());
  SYSLOG(INFO) << "Starting upload.";

  if (!SetUpMultipart()) {
    SYSLOG(ERROR) << "Multipart message assembly failed.";
    state_ = ERROR;
    return;
  }
  CreateAndStartURLLoader(access_token_);
  state_ = UPLOADING;
}

void UploadJobImpl::OnGetTokenSuccess(
    const OAuth2AccessTokenManager::Request* request,
    const OAuth2AccessTokenConsumer::TokenResponse& token_response) {
  DCHECK_EQ(ACQUIRING_TOKEN, state_);
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  SYSLOG(INFO) << "Token successfully acquired.";

  // Also cache the token locally, so that we can revoke it later if necessary.
  access_token_ = token_response.access_token;
  StartUpload();
}

void UploadJobImpl::OnGetTokenFailure(
    const OAuth2AccessTokenManager::Request* request,
    const GoogleServiceAuthError& error) {
  DCHECK_EQ(ACQUIRING_TOKEN, state_);
  DCHECK_EQ(access_token_request_.get(), request);
  access_token_request_.reset();
  SYSLOG(ERROR) << "Token request failed: " << error.ToString();
  HandleError(AUTHENTICATION_ERROR);
}

void UploadJobImpl::HandleError(ErrorCode error_code) {
  retry_++;
  url_loader_.reset();

  SYSLOG(ERROR) << "Upload failed, error code: " << error_code;

  if (retry_ >= kMaxAttempts) {
    // Maximum number of attempts reached, failure.
    SYSLOG(ERROR) << "Maximum number of attempts reached.";
    access_token_.clear();
    post_data_.reset();
    state_ = ERROR;
    delegate_->OnFailure(error_code);
  } else {
    if (error_code == AUTHENTICATION_ERROR) {
      SYSLOG(ERROR) << "Retrying upload with a new token.";
      // Request new token and retry.
      OAuth2AccessTokenManager::ScopeSet scope_set;
      scope_set.insert(GaiaConstants::kDeviceManagementServiceOAuth);
      access_token_manager_->InvalidateAccessToken(account_id_, scope_set,
                                                   access_token_);
      access_token_.clear();
      task_runner_->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&UploadJobImpl::RequestAccessToken,
                         weak_factory_.GetWeakPtr()),
          base::Milliseconds(g_retry_delay_ms));
    } else {
      // Retry without a new token.
      state_ = ACQUIRING_TOKEN;
      SYSLOG(WARNING) << "Retrying upload with the same token.";
      task_runner_->PostDelayedTask(FROM_HERE,
                                    base::BindOnce(&UploadJobImpl::StartUpload,
                                                   weak_factory_.GetWeakPtr()),
                                    base::Milliseconds(g_retry_delay_ms));
    }
  }
}

void UploadJobImpl::OnURLLoadComplete(
    scoped_refptr<net::HttpResponseHeaders> headers) {
  DCHECK_EQ(UPLOADING, state_);

  SYSLOG(INFO) << "URL fetch completed.";
  std::unique_ptr<network::SimpleURLLoader> url_loader = std::move(url_loader_);

  if (!headers) {
    SYSLOG(ERROR) << "SimpleURLLoader error " << url_loader->NetError();
    HandleError(NETWORK_ERROR);
  } else if (headers->response_code() == net::HTTP_OK) {
    // Successful upload
    access_token_.clear();
    post_data_.reset();
    state_ = SUCCESS;
    delegate_->OnSuccess();
  } else if (headers->response_code() == net::HTTP_UNAUTHORIZED) {
    SYSLOG(ERROR) << "Unauthorized request.";
    HandleError(AUTHENTICATION_ERROR);
  } else {
    SYSLOG(ERROR) << "POST request failed with HTTP status code "
                  << headers->response_code() << ".";
    HandleError(SERVER_ERROR);
  }
}

}  // namespace policy
