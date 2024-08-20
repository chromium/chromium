// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/policy/messaging_layer/upload/file_upload_impl.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/thread_annotations.h"
#include "base/types/expected.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_identity/device_oauth2_token_service.h"
#include "chrome/browser/device_identity/device_oauth2_token_service_factory.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/messaging_layer/upload/file_upload_job.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/reporting_errors.h"
#include "components/reporting/util/status.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/core_account_id.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_access_token_manager.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace reporting {

constexpr char kAuthorizationPrefix[] = "Bearer ";

constexpr char kUploadStatusHeader[] = "X-Goog-Upload-Status";
constexpr char kUploadCommandHeader[] = "X-Goog-Upload-Command";
constexpr char kUploadHeaderContentLengthHeader[] =
    "X-Goog-Upload-Header-Content-Length";
constexpr char kUploadHeaderContentTypeHeader[] =
    "X-Goog-Upload-Header-Content-Type";
constexpr char kUploadChunkGranularityHeader[] =
    "X-Goog-Upload-Chunk-Granularity";
constexpr char kUploadUrlHeader[] = "X-Goog-Upload-Url";
constexpr char kUploadSizeReceivedHeader[] = "X-Goog-Upload-Size-Received";
constexpr char kUploadOffsetHeader[] = "X-Goog-Upload-Offset";
constexpr char kUploadProtocolHeader[] = "X-Goog-Upload-Protocol";
constexpr char kUploadIdHeader[] = "X-GUploader-UploadID";

// Helper for network response, headers analysis and status retrieval.
StatusOr<std::string> CheckResponseAndGetStatus(
    const std::unique_ptr<::network::SimpleURLLoader> url_loader,
    const scoped_refptr<::net::HttpResponseHeaders> headers) {
  if (!headers) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::NO_HEADERS_FOUND,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::DATA_LOSS,
               base::StrCat({"Network error=",
                             ::net::ErrorToString(url_loader->NetError())})));
  }

  if (headers->response_code() == net::HTTP_OK) {
    // Successful upload, retrieve and return upload status.
    std::string upload_status;
    if (!headers->GetNormalizedHeader(kUploadStatusHeader, &upload_status)) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::UNEXPECTED_UPLOAD_STATUS,
          DataLossErrorReason::MAX_VALUE);
      return base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status})));
    }
    return upload_status;
  } else if (headers->response_code() == net::HTTP_UNAUTHORIZED) {
    return base::unexpected(
        Status(error::UNAUTHENTICATED, "Authentication error"));
  } else {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::POST_REQUEST_FAILED,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::DATA_LOSS,
               base::StrCat({"POST request failed with HTTP status code ",
                             base::NumberToString(headers->response_code())})));
  }
}

// Helper to learn upload chunk granularity.
StatusOr<int64_t> GetChunkGranularity(
    const scoped_refptr<::net::HttpResponseHeaders> headers) {
  int64_t upload_granularity = -1;
  std::string upload_granularity_string;
  if (!headers->GetNormalizedHeader(kUploadChunkGranularityHeader,
                                    &upload_granularity_string)) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::NO_GRANULARITTY_RETURNED,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(
        Status(error::DATA_LOSS, "No granularity returned"));
  }
  if (!base::StringToInt64(upload_granularity_string, &upload_granularity) ||
      upload_granularity <= 0L) {
    base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                  DataLossErrorReason::UNEXPECTED_GRANULARITY,
                                  DataLossErrorReason::MAX_VALUE);
    return base::unexpected(Status(
        error::DATA_LOSS,
        base::StrCat({"Unexpected granularity=", upload_granularity_string})));
  }
  return upload_granularity;
}

// Generic context that returns result and self-destructs.
template <typename R>
class ActionContext {
 public:
  ActionContext(const ActionContext& other) = delete;
  ActionContext& operator=(const ActionContext& other) = delete;
  virtual ~ActionContext() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(!result_cb_) << "Destruct before callback";
  }

 protected:
  // Constructor is only available to derived classes.
  ActionContext(base::WeakPtr<FileUploadDelegate> delegate,
                base::OnceCallback<void(R)> result_cb)
      : delegate_(std::move(delegate)), result_cb_(std::move(result_cb)) {}

  // Completes work returning result or status, and then self-destructs.
  // This is the only way `ActionContext` ceases to exist, so any asynchronous
  // callback in its subclasses is safe to use `base::Unretained(this)` and
  // does not need weak pointers.
  void Complete(R result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK(result_cb_) << "Already completed";
    std::move(result_cb_).Run(std::move(result));
    delete this;
  }

  // Accessor.
  base::WeakPtr<FileUploadDelegate> delegate() const { return delegate_; }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  const base::WeakPtr<FileUploadDelegate> delegate_;
  base::OnceCallback<void(R)> result_cb_ GUARDED_BY_CONTEXT(sequence_checker_);
};

// Self-destructing context for Authentication.
class FileUploadDelegate::AccessTokenRetriever
    : public ActionContext<StatusOr<std::string>>,
      public OAuth2AccessTokenManager::Consumer {
 public:
  AccessTokenRetriever(
      base::WeakPtr<FileUploadDelegate> delegate,
      base::OnceCallback<void(StatusOr<std::string>)> result_cb)
      : ActionContext(std::move(delegate), std::move(result_cb)),
        OAuth2AccessTokenManager::Consumer("cros_upload_job") {}

  void RequestAccessToken() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    CHECK(!access_token_request_);
    DVLOG(1) << "Requesting access token.";

    access_token_request_ = delegate()->StartOAuth2Request(this);
  }

 private:
  // OAuth2AccessTokenManager::Consumer:
  void OnGetTokenSuccess(
      const OAuth2AccessTokenManager::Request* request,
      const OAuth2AccessTokenConsumer::TokenResponse& token_response) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(access_token_request_.get(), request);
    access_token_request_.reset();
    DVLOG(1) << "Token successfully acquired.";
    Complete(token_response.access_token);
  }

  void OnGetTokenFailure(const OAuth2AccessTokenManager::Request* request,
                         const GoogleServiceAuthError& error) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    CHECK_EQ(access_token_request_.get(), request);
    access_token_request_.reset();
    LOG(ERROR) << "Token request failed: " << error.ToString();
    Complete(
        base::unexpected(Status(error::UNAUTHENTICATED, error.ToString())));
  }

  // The OAuth request to receive the access token.
  std::unique_ptr<OAuth2AccessTokenManager::Request> access_token_request_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

// Self-destructing context for FileUploadJob initiation.
class FileUploadDelegate::InitContext
    : public ActionContext<StatusOr<
          std::pair<int64_t /*total*/, std::string /*session_token*/>>> {
 public:
  InitContext(
      std::string_view origin_path,
      std::string_view upload_parameters,
      std::string_view access_token,
      base::WeakPtr<FileUploadDelegate> delegate,
      base::OnceCallback<
          void(StatusOr<std::pair<int64_t /*total*/,
                                  std::string /*session_token*/>>)> result_cb)
      : ActionContext(std::move(delegate), std::move(result_cb)),
        origin_path_(origin_path),
        upload_parameters_(upload_parameters),
        access_token_(access_token) {}

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    // Perform file operation on a thread pool, then resume on the current task
    // runner.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&InitContext::InitFile, origin_path_),
        base::BindOnce(&InitContext::FileOpened, base::Unretained(this)));
  }

  void FileOpened(StatusOr<int64_t> total_result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    if (!total_result.has_value()) {
      Complete(base::unexpected(total_result.error()));
      return;
    }

    // Record total size of the file.
    total_ = total_result.value();

    // Initiate upload.
    DVLOG(1) << "Starting URL fetcher.";

    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->headers.SetHeader(
        ::net::HttpRequestHeaders::kAuthorization,
        base::StrCat({kAuthorizationPrefix, access_token_.c_str()}));
    resource_request->headers.SetHeader(kUploadCommandHeader, "start");
    resource_request->headers.SetHeader(kUploadHeaderContentLengthHeader,
                                        base::NumberToString(total_));
    resource_request->headers.SetHeader(kUploadHeaderContentTypeHeader,
                                        "application/octet-stream");

    url_loader_ = delegate()->CreatePostLoader(std::move(resource_request));

    // Construct and attach medatata - see
    // go/scotty-http-protocols#unified-resumable-protocol
    // Here we expect `upload_parameters` to be in the form like:
    //
    // "<File-Type>\r\n"
    // "  support_file\r\n"
    // "</File-Type>\r\n"
    // "<Command-ID>\r\n"
    // "  ID12345\r\n"
    // "</Command-ID>\r\n"
    // "<Filename>\r\n"
    // "  resulting_file_name\r\n"
    // "</Filename>\r\n"
    // "text/xml"
    //
    // with the last line indicating content type.
    const auto pos = upload_parameters_.find_last_of("\n");
    if (pos == std::string::npos || pos + 1u >= upload_parameters_.size()) {
      Complete(base::unexpected(
          Status(error::INVALID_ARGUMENT,
                 base::StrCat({"Cannot parse upload_parameters=`",
                               upload_parameters_, "`"}))));
      return;
    }
    const std::string metadata_contents_type =
        upload_parameters_.substr(pos + 1u);
    const std::string metadata =
        upload_parameters_.substr(0, pos + 1u);  // With \n included!
    url_loader_->AttachStringForUpload(metadata, metadata_contents_type);

    // Make a call and get response headers.
    delegate()->SendAndGetResponse(
        url_loader_.get(), base::BindOnce(&InitContext::OnInitURLLoadComplete,
                                          base::Unretained(this)));
  }

  void OnInitURLLoadComplete(
      scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto status_result =
        CheckResponseAndGetStatus(std::move(url_loader_), headers);
    if (!status_result.has_value()) {
      Complete(base::unexpected(std::move(status_result).error()));
      return;
    }

    const std::string upload_status = status_result.value();
    if (!base::EqualsCaseInsensitiveASCII(upload_status, "active")) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::UNEXPECTED_UPLOAD_STATUS,
          DataLossErrorReason::MAX_VALUE);
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status}))));
      return;
    }

    // Just make sure granulatiy is returned, do not use it here.
    auto upload_granularity_result = GetChunkGranularity(headers);
    if (!upload_granularity_result.has_value()) {
      Complete(base::unexpected(std::move(upload_granularity_result).error()));
      return;
    }

    std::string upload_url;
    if (!headers->GetNormalizedHeader(kUploadUrlHeader, &upload_url)) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::NO_UPLOAD_URL_RETURNED,
                                    DataLossErrorReason::MAX_VALUE);
      Complete(
          base::unexpected(Status(error::DATA_LOSS, "No upload URL returned")));
      return;
    }

    Complete(
        std::make_pair(total_, base::StrCat({origin_path_, "\n", upload_url})));
  }

  static StatusOr<int64_t> InitFile(const std::string origin_path) {
    auto handle = std::make_unique<base::File>(
        base::FilePath(origin_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!handle->IsValid()) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_OPEN_UPLOAD_FILE,
          DataLossErrorReason::MAX_VALUE);
      return base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"Cannot open file=", origin_path, " error=",
                        base::File::ErrorToString(handle->error_details())})));
    }

    // Calculate total size of the file.
    return handle->GetLength();
  }

 private:
  const std::string origin_path_;
  const std::string upload_parameters_;
  const std::string access_token_;

  // Helper to upload the data.
  std::unique_ptr<::network::SimpleURLLoader> url_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Total size.
  int64_t total_ GUARDED_BY_CONTEXT(sequence_checker_) = 0L;
};

// Self-destructing context for FileUploadJob next step.
class FileUploadDelegate::NextStepContext
    : public ActionContext<StatusOr<
          std::pair<int64_t /*uploaded*/, std::string /*session_token*/>>> {
 public:
  NextStepContext(
      int64_t total,
      int64_t uploaded,
      std::string_view session_token,
      ScopedReservation scoped_reservation,
      base::WeakPtr<FileUploadDelegate> delegate,
      base::OnceCallback<
          void(StatusOr<std::pair<int64_t /*uploaded*/,
                                  std::string /*session_token*/>>)> result_cb)
      : ActionContext(std::move(delegate), std::move(result_cb)),
        total_(total),
        uploaded_(uploaded),
        session_token_(session_token),
        scoped_reservation_(std::move(scoped_reservation)) {}

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    // Parse session token.
    const auto tokens = base::SplitStringPiece(
        session_token_, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (tokens.size() != 2 || tokens[0].empty() || tokens[1].empty()) {
      Complete(base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"Corrupt session token `", session_token_, "`"}))));
      return;
    }
    origin_path_ = tokens[0];
    resumable_upload_url_ = GURL(tokens[1]);
    if (!resumable_upload_url_.is_valid()) {
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Corrupt resumable upload URL=", tokens[1]}))));
      return;
    }

    // Query upload.
    DVLOG(1) << "Starting Query URL fetcher.";
    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->url = resumable_upload_url_;
    resource_request->headers.SetHeader(kUploadCommandHeader, "query");

    url_loader_ = delegate()->CreatePostLoader(std::move(resource_request));

    // Make a call and get response headers.
    delegate()->SendAndGetResponse(
        url_loader_.get(),
        base::BindOnce(&NextStepContext::OnQueryURLLoadComplete,
                       base::Unretained(this)));
  }

  void OnQueryURLLoadComplete(
      scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    auto status_result =
        CheckResponseAndGetStatus(std::move(url_loader_), headers);
    if (!status_result.has_value()) {
      Complete(base::unexpected(std::move(status_result).error()));
      return;
    }

    const std::string upload_status = status_result.value();
    if (base::EqualsCaseInsensitiveASCII(upload_status, "final")) {
      // Already done.
      Complete(std::make_pair(total_, session_token_));
      return;
    }
    if (!base::EqualsCaseInsensitiveASCII(upload_status, "active")) {
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status}))));
      return;
    }

    int64_t upload_received = -1;
    {
      std::string upload_received_string;
      if (!headers->GetNormalizedHeader(kUploadSizeReceivedHeader,
                                        &upload_received_string)) {
        Complete(base::unexpected(
            Status(error::DATA_LOSS, "No upload size returned")));
        return;
      }
      if (!base::StringToInt64(upload_received_string, &upload_received) ||
          upload_received < 0 || uploaded_ > upload_received) {
        Complete(base::unexpected(Status(
            error::DATA_LOSS,
            base::StrCat({"Unexpected received=", upload_received_string,
                          ", expected=", base::NumberToString(uploaded_)}))));
        return;
      }
    }
    if (upload_received >= total_) {
      // Already done.
      Complete(std::make_pair(total_, session_token_));
      return;
    }

    auto upload_granularity_result = GetChunkGranularity(headers);
    if (!upload_granularity_result.has_value()) {
      Complete(base::unexpected(std::move(upload_granularity_result).error()));
      return;
    }
    auto upload_granularity = upload_granularity_result.value();

    // Determine maximum buffer size, rounded down to upload_granularity.
    DCHECK_CALLED_ON_VALID_SEQUENCE(delegate()->sequence_checker_);
    int64_t max_size =
        (delegate()->max_upload_buffer_size_ / upload_granularity) *
        upload_granularity;

    // Upload next or last chunk.
    DVLOG(1) << "Starting Upload URL fetcher.";
    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->url = resumable_upload_url_;
    int64_t size = total_ - upload_received;
    if (size < max_size) {
      resource_request->headers.SetHeader(kUploadCommandHeader,
                                          "upload, finalize");
    } else {
      size = max_size;
      resource_request->headers.SetHeader(kUploadCommandHeader, "upload");
    }
    resource_request->headers.SetHeader(kUploadOffsetHeader,
                                        base::NumberToString(upload_received));

    // See whether we have enough memory for the buffer.
    ScopedReservation buffer_reservation(size, scoped_reservation_);
    if (!buffer_reservation.reserved()) {
      // Do not post error status - it would stop the whole job.
      // Post success without making any progress!
      Complete(std::make_pair(upload_received, session_token_));
      return;
    }
    // Attach the new reservation.
    scoped_reservation_.HandOver(buffer_reservation);

    // Retrieve data from the file to be attached on a thread pool, then resume
    // on the current task runner. Note: it could be done with
    // `AttachFileForUpload` instead, but loading into memory allows to check
    // integrity of the file (TBD; for now we only verify file access and
    // size).
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&NextStepContext::LoadFileData,
                       std::string(origin_path_), total_, upload_received,
                       size),
        base::BindOnce(&NextStepContext::PerformUpload, base::Unretained(this),
                       upload_received, size, std::move(resource_request)));
  }

  void PerformUpload(
      int64_t upload_received,
      int64_t size,
      std::unique_ptr<::network::ResourceRequest> resource_request,
      StatusOr<std::string> buffer_result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    if (!buffer_result.has_value()) {
      Complete(base::unexpected(std::move(buffer_result).error()));
      return;
    }

    url_loader_ = delegate()->CreatePostLoader(std::move(resource_request));
    url_loader_->AttachStringForUpload(
        buffer_result.value(),  // owned by caller!
        "application/octet-stream");

    // Make a call and get response headers.
    delegate()->SendAndGetResponse(
        url_loader_.get(),
        base::BindOnce(&NextStepContext::OnUploadURLLoadComplete,
                       base::Unretained(this), upload_received, size));
  }

  void OnUploadURLLoadComplete(
      int64_t uploaded,
      int64_t size,
      scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Buffer no longer used.
    scoped_reservation_.Reduce(size);

    auto status_result =
        CheckResponseAndGetStatus(std::move(url_loader_), headers);
    if (!status_result.has_value()) {
      Complete(base::unexpected(status_result.error()));
      return;
    }

    const std::string upload_status = status_result.value();
    if (base::EqualsCaseInsensitiveASCII(upload_status, "final")) {
      // Already done.
      Complete(std::make_pair(total_, session_token_));
      return;
    }
    if (!base::EqualsCaseInsensitiveASCII(upload_status, "active")) {
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status}))));
      return;
    }

    Complete(std::make_pair(uploaded + size, session_token_));
  }

  static StatusOr<std::string> LoadFileData(const std::string origin_path,
                                            int64_t total,
                                            int64_t offset,
                                            int64_t size) {
    // Retrieve data from the file to be attached. Note: it could be done with
    // `AttachFileForUpload` instead, but loading into memory allows to check
    // integrity of the file (TBD; for now we only verify file access and
    // size).
    std::string buffer;

    auto handle = std::make_unique<base::File>(
        base::FilePath(origin_path),
        base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!handle->IsValid()) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::FAILED_TO_OPEN_UPLOAD_FILE,
          DataLossErrorReason::MAX_VALUE);
      return base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"Cannot open file=", origin_path, " error=",
                        base::File::ErrorToString(handle->error_details())})));
    }

    // Verify total size of the file.
    if (total != handle->GetLength()) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::FILE_SIZE_MISMATCH,
                                    DataLossErrorReason::MAX_VALUE);
      return base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"File=", origin_path, " changed size ", " from ",
                               base::NumberToString(total), " to ",
                               base::NumberToString(handle->GetLength())})));
    }

    // Load into buffer.
    buffer.resize(
        size);  // Initialization is redundant, but std::string mandates it.
    const int read_size = handle->Read(offset, buffer.data(), size);
    if (read_size < 0) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::CANNOT_READ_FILE,
                                    DataLossErrorReason::MAX_VALUE);
      return base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"Cannot read file=", origin_path, " error=",
                        base::File::ErrorToString(handle->error_details())})));
    }
    if (read_size != size) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::CANNOT_READ_FILE,
                                    DataLossErrorReason::MAX_VALUE);
      return base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Failed to read file=", origin_path,
                               " offset=", base::NumberToString(offset),
                               " size=", base::NumberToString(size),
                               " read=", base::NumberToString(read_size)})));
    }
    return buffer;
  }

 private:
  const int64_t total_;
  const int64_t uploaded_;
  const std::string session_token_;

  // Session token components.
  std::string_view origin_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  GURL resumable_upload_url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Helper to upload the data.
  std::unique_ptr<network::SimpleURLLoader> url_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Memory usage by upload.
  ScopedReservation scoped_reservation_ GUARDED_BY_CONTEXT(sequence_checker_);
};

// Self-destructing context for FileUploadJob finalization.
class FileUploadDelegate::FinalContext
    : public ActionContext<StatusOr<std::string /*access_parameters*/>> {
 public:
  FinalContext(
      std::string_view session_token,
      base::WeakPtr<FileUploadDelegate> delegate,
      base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
          result_cb)
      : ActionContext(std::move(delegate), std::move(result_cb)),
        session_token_(session_token) {}

  void Run() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    // Parse session token.
    const auto tokens = base::SplitStringPiece(
        session_token_, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (tokens.size() != 2 || tokens[0].empty() || tokens[1].empty()) {
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::CORRUPT_SESSION_TOKEN,
                                    DataLossErrorReason::MAX_VALUE);
      Complete(base::unexpected(Status(
          error::DATA_LOSS,
          base::StrCat({"Corrupt session token `", session_token_, "`"}))));
      return;
    }
    origin_path_ = tokens[0];
    resumable_upload_url_ = GURL(tokens[1]);
    if (!resumable_upload_url_.is_valid()) {
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::CORRUPT_RESUMABLE_UPLOAD_URL,
          DataLossErrorReason::MAX_VALUE);
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Corrupt resumable upload URL=", tokens[1]}))));
      return;
    }

    // Query upload.
    DVLOG(1) << "Starting Query URL fetcher.";
    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->url = resumable_upload_url_;
    resource_request->headers.SetHeader(kUploadCommandHeader, "query");

    url_loader_ = delegate()->CreatePostLoader(std::move(resource_request));

    // Make a call and get response headers.
    delegate()->SendAndGetResponse(
        url_loader_.get(), base::BindOnce(&FinalContext::OnQueryURLLoadComplete,
                                          base::Unretained(this)));
  }

  void OnQueryURLLoadComplete(
      scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!delegate()) {
      Complete(base::unexpected(
          Status(error::UNAVAILABLE, "Delegate is unavailable")));
      base::UmaHistogramEnumeration(
          reporting::kUmaUnavailableErrorReason,
          UnavailableErrorReason::FILE_UPLOAD_DELEGATE_IS_NULL,
          UnavailableErrorReason::MAX_VALUE);
      return;
    }

    auto status_result =
        CheckResponseAndGetStatus(std::move(url_loader_), headers);
    if (!status_result.has_value()) {
      Complete(base::unexpected(std::move(status_result).error()));
      return;
    }

    const std::string& upload_status = status_result.value();
    if (base::EqualsCaseInsensitiveASCII(upload_status, "final")) {
      // All done.
      RespondOnFinal(headers);
      return;
    }
    if (!base::EqualsCaseInsensitiveASCII(upload_status, "active")) {
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status}))));
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::UNEXPECTED_UPLOAD_STATUS,
          DataLossErrorReason::MAX_VALUE);
      return;
    }

    int64_t upload_received = -1;
    {
      std::string upload_received_string;
      if (!headers->GetNormalizedHeader(kUploadSizeReceivedHeader,
                                        &upload_received_string)) {
        Complete(base::unexpected(
            Status(error::DATA_LOSS, "No upload size returned")));
        base::UmaHistogramEnumeration(
            reporting::kUmaDataLossErrorReason,
            DataLossErrorReason::NO_UPLOAD_SIZE_RETURNED,
            DataLossErrorReason::MAX_VALUE);
        return;
      }
      if (!base::StringToInt64(upload_received_string, &upload_received) ||
          upload_received < 0) {
        Complete(base::unexpected(Status(
            error::DATA_LOSS,
            base::StrCat({"Unexpected received=", upload_received_string}))));
        base::UmaHistogramEnumeration(
            reporting::kUmaDataLossErrorReason,
            DataLossErrorReason::UNEXPECTED_UPLOAD_RECEIVED_CODE,
            DataLossErrorReason::MAX_VALUE);
        return;
      }
    }

    // Finalize upload.
    DVLOG(1) << "Starting Upload URL fetcher.";
    auto resource_request = std::make_unique<::network::ResourceRequest>();
    resource_request->url = resumable_upload_url_;
    resource_request->headers.SetHeader(kUploadCommandHeader, "finalize");

    url_loader_ = delegate()->CreatePostLoader(std::move(resource_request));

    // Make a call and get response headers.
    delegate()->SendAndGetResponse(
        url_loader_.get(),
        base::BindOnce(&FinalContext::OnFinalizeURLLoadComplete,
                       base::Unretained(this)));
  }

  void OnFinalizeURLLoadComplete(
      scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto status_result =
        CheckResponseAndGetStatus(std::move(url_loader_), headers);
    if (!status_result.has_value()) {
      Complete(base::unexpected(std::move(status_result).error()));
      return;
    }

    const std::string upload_status = status_result.value();
    if (!base::EqualsCaseInsensitiveASCII(upload_status, "final")) {
      Complete(base::unexpected(
          Status(error::DATA_LOSS,
                 base::StrCat({"Unexpected upload status=", upload_status}))));
      base::UmaHistogramEnumeration(
          reporting::kUmaDataLossErrorReason,
          DataLossErrorReason::UNEXPECTED_UPLOAD_STATUS,
          DataLossErrorReason::MAX_VALUE);
      return;
    }

    RespondOnFinal(headers);
  }

 private:
  void RespondOnFinal(scoped_refptr<::net::HttpResponseHeaders> headers) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    std::string upload_id;
    if (!headers->GetNormalizedHeader(kUploadIdHeader, &upload_id) ||
        upload_id.empty()) {
      Complete(
          base::unexpected(Status(error::DATA_LOSS, "No upload ID returned")));
      base::UmaHistogramEnumeration(reporting::kUmaDataLossErrorReason,
                                    DataLossErrorReason::NO_UPLOAD_ID_RETURNED,
                                    DataLossErrorReason::MAX_VALUE);
      return;
    }

    Complete(base::StrCat({"Upload_id=", upload_id}));
  }

  const std::string session_token_;

  // Session token components.
  std::string_view origin_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  GURL resumable_upload_url_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Helper to upload the data.
  std::unique_ptr<network::SimpleURLLoader> url_loader_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

FileUploadDelegate::FileUploadDelegate() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

FileUploadDelegate::~FileUploadDelegate() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void FileUploadDelegate::InitializeOnce() {
  DCHECK_CURRENTLY_ON(::content::BrowserThread::UI);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (url_loader_factory_) {
    return;  // Already initialized.
  }

  upload_url_ = GURL(g_browser_process->browser_policy_connector()
                         ->GetFileStorageServerUploadUrl());
  CHECK(upload_url_.is_valid());

  account_id_ = DeviceOAuth2TokenServiceFactory::Get()->GetRobotAccountId();
  access_token_manager_ =
      DeviceOAuth2TokenServiceFactory::Get()->GetAccessTokenManager();
  CHECK(access_token_manager_);
  url_loader_factory_ = g_browser_process->shared_url_loader_factory();
  CHECK(url_loader_factory_);
  traffic_annotation_ = std::make_unique<::net::NetworkTrafficAnnotationTag>(
      ::net::DefineNetworkTrafficAnnotation("chrome_support_tool_file_upload",
                                            R"(
        semantics {
          sender: "ChromeOS Support Tool"
          description:
              "ChromeOS Support Tool can request log files upload on a managed "
              "device on behalf of the admin. The log files are bundled "
              "together in a single zip archive that is uploaded using "
              "a multi-chunk resumable protocol. They are stored on Google "
              "servers, so the admin can view the logs in the Admin Console."
          trigger: "When UPLOAD_LOG event is posted by Support Tool "
                   "on the admin's request."
          data: "Zipped archive of log files created by Support Tool and "
                "placed in /var/spool/."
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "lbaraz@chromium.org"
            }
            contacts {
              email: "cros-reporting-team@google.com"
            }
          }
          user_data {
            type: USER_CONTENT
          }
          last_reviewed: "2023-03-16"
        }
        policy {
          cookies_allowed: NO
          setting: "This feature cannot be disabled in settings."
          chrome_device_policy {
            # LogUploadEnabled
            device_log_upload_settings {
              system_log_upload_enabled: false
            }
          }
        }
      )"));

  max_upload_buffer_size_ = kMaxUploadBufferSize;
}

std::unique_ptr<OAuth2AccessTokenManager::Request>
FileUploadDelegate::StartOAuth2Request(
    OAuth2AccessTokenManager::Consumer* consumer) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OAuth2AccessTokenManager::ScopeSet scope_set;
  scope_set.insert(GaiaConstants::kDeviceManagementServiceOAuth);
  return access_token_manager_->StartRequest(account_id_, scope_set, consumer);
}

std::unique_ptr<::network::SimpleURLLoader>
FileUploadDelegate::CreatePostLoader(
    std::unique_ptr<::network::ResourceRequest> resource_request) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  resource_request->method = "POST";
  if (resource_request->url.is_empty()) {
    resource_request->url = upload_url_;
  }
  resource_request->headers.SetHeader(kUploadProtocolHeader, "resumable");
  return ::network::SimpleURLLoader::Create(std::move(resource_request),
                                            *traffic_annotation_);
}

void FileUploadDelegate::SendAndGetResponse(
    ::network::SimpleURLLoader* url_loader,
    base::OnceCallback<void(scoped_refptr<::net::HttpResponseHeaders> headers)>
        response_cb) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  url_loader->DownloadHeadersOnly(
      url_loader_factory_.get(),
      base::BindPostTaskToCurrentDefault(std::move(response_cb)));
}

// static
void FileUploadDelegate::DoInitiate(
    std::string_view origin_path,
    std::string_view upload_parameters,
    base::OnceCallback<void(
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>)>
        result_cb) {
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileUploadDelegate::DoInitiate, GetWeakPtr(),
            std::string(origin_path), std::string(upload_parameters),
            base::BindPostTaskToCurrentDefault(std::move(result_cb))));
    return;
  }

  InitializeOnce();

  DVLOG(1) << "Creating file upload job for support tool use";

  (new AccessTokenRetriever(
       GetWeakPtr(),
       base::BindPostTaskToCurrentDefault(base::BindOnce(
           &FileUploadDelegate::OnAccessTokenResult, GetWeakPtr(),
           std::string(origin_path), std::string(upload_parameters),
           std::move(result_cb)))))
      ->RequestAccessToken();
}

void FileUploadDelegate::OnAccessTokenResult(
    std::string_view origin_path,
    std::string_view upload_parameters,
    base::OnceCallback<void(
        StatusOr<std::pair<int64_t /*total*/, std::string /*session_token*/>>)>
        result_cb,
    StatusOr<std::string> access_token_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!access_token_result.has_value()) {
    std::move(result_cb).Run(
        base::unexpected(std::move(access_token_result).error()));
    return;
  }

  // Measure file size and store it in total.
  (new InitContext(origin_path, upload_parameters, access_token_result.value(),
                   GetWeakPtr(), std::move(result_cb)))
      ->Run();
}

void FileUploadDelegate::DoNextStep(
    int64_t total,
    int64_t uploaded,
    std::string_view session_token,
    ScopedReservation scoped_reservation,
    base::OnceCallback<void(StatusOr<std::pair<int64_t /*uploaded*/,
                                               std::string /*session_token*/>>)>
        result_cb) {
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileUploadDelegate::DoNextStep, GetWeakPtr(), total, uploaded,
            std::string(session_token), std::move(scoped_reservation),
            base::BindPostTaskToCurrentDefault(std::move(result_cb))));
    return;
  }

  InitializeOnce();

  (new NextStepContext(total, uploaded, session_token,
                       std::move(scoped_reservation), GetWeakPtr(),
                       std::move(result_cb)))
      ->Run();
}

void FileUploadDelegate::DoFinalize(
    std::string_view session_token,
    base::OnceCallback<void(StatusOr<std::string /*access_parameters*/>)>
        result_cb) {
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&FileUploadDelegate::DoFinalize, GetWeakPtr(),
                                  std::string(session_token),
                                  base::BindPostTaskToCurrentDefault(
                                      std::move(result_cb))));
    return;
  }

  InitializeOnce();

  (new FinalContext(session_token, GetWeakPtr(), std::move(result_cb)))->Run();
}

void FileUploadDelegate::DoDeleteFile(std::string_view origin_path) {
  const auto delete_result = base::DeleteFile(base::FilePath(origin_path));
  if (!delete_result) {
    LOG(WARNING) << "Failed to delete file=" << origin_path;
  }
}

base::WeakPtr<FileUploadDelegate> FileUploadDelegate::GetWeakPtr() {
  CHECK(weak_ptr_factory_) << "Factory already invalidated.";
  return weak_ptr_factory_->GetWeakPtr();
}
}  // namespace reporting
