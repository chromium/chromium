// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/api_client.h"

#include <optional>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/indigo/indigo_metrics.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "net/base/data_url.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"

namespace indigo {

namespace {

const char* GetErrorPrefix(google_apis::ApiErrorCode code) {
  switch (MapApiErrorCodeToStatus(code)) {
    case IndigoApiStatus::kHttpError:
      return "HTTP error: ";
    case IndigoApiStatus::kNetworkError:
      return "Network error: ";
    case IndigoApiStatus::kParseError:
      return "Parse error: ";
    case IndigoApiStatus::kCancelled:
      return "Cancelled: ";
    default:
      return "Other error: ";
  }
}

class GenerateRequest : public google_apis::UrlFetchRequestBase {
 public:
  GenerateRequest(google_apis::RequestSender* sender,
                  GURL url,
                  base::span<const uint8_t> product_image_bytes,
                  ApiClient::GenerateCallback callback)
      : google_apis::UrlFetchRequestBase(sender,
                                         google_apis::ProgressCallback(),
                                         google_apis::ProgressCallback()),
        url_(url),
        callback_(std::move(callback)),
        start_time_(base::TimeTicks::Now()) {
    base::DictValue request_dict;
    request_dict.Set("productImageBytes",
                     base::Base64Encode(product_image_bytes));
    request_dict.Set("outputFormat", "OUTPUT_FORMAT_IMAGE_BYTES");
    base::JSONWriter::Write(request_dict, &request_content_);
  }

  GenerateRequest(const GenerateRequest&) = delete;
  GenerateRequest& operator=(const GenerateRequest&) = delete;
  ~GenerateRequest() override {
    if (callback_) {
      RecordApiStatusAndLatency(IndigoApiEndpoint::kGenerate,
                                IndigoApiStatus::kCancelled,
                                base::TimeTicks::Now() - start_time_);
      std::move(callback_).Run(
          base::unexpected(GenerateImageError{"Request cancelled"}));
    }
  }

 protected:
  GURL GetURL() const override { return url_; }

  google_apis::HttpRequestMethod GetRequestType() const override {
    return google_apis::HttpRequestMethod::kPost;
  }

  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override {
    *upload_content_type = "application/json";
    *upload_content = request_content_;
    return true;
  }

  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override {
    return code;
  }

  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override {
    return error == google_apis::HTTP_SUCCESS;
  }

  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kGenerate, GetErrorCode());

    auto complete =
        [this](base::expected<GeneratedImage, GenerateImageError> result,
               IndigoApiStatus status) {
          RecordApiStatusAndLatency(IndigoApiEndpoint::kGenerate, status,
                                    base::TimeTicks::Now() - start_time_);
          std::move(callback_).Run(std::move(result));
          OnProcessURLFetchResultsComplete();
        };

    if (GetErrorCode() != google_apis::HTTP_SUCCESS) {
      return complete(base::unexpected(GenerateImageError{
                          GetErrorPrefix(GetErrorCode()) +
                          google_apis::ApiErrorCodeToString(GetErrorCode())}),
                      MapApiErrorCodeToStatus(GetErrorCode()));
    }

    auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
        response_body, base::JSON_PARSE_RFC);
    if (!parse_result.has_value()) {
      return complete(base::unexpected(GenerateImageError{
                          "Invalid JSON response from " + url_.spec() + ": " +
                          parse_result.error().ToString()}),
                      IndigoApiStatus::kParseError);
    }
    if (!parse_result->is_dict()) {
      return complete(base::unexpected(GenerateImageError{
                          "Invalid JSON response from " + url_.spec() +
                          ": not a dictionary"}),
                      IndigoApiStatus::kInvalidResponse);
    }

    const auto& dict = parse_result->GetDict();
    const auto* result = dict.FindDict("result");
    if (result) {
      const std::string* url_string = result->FindString("generatedImageUrl");
      if (!url_string) {
        return complete(base::unexpected(GenerateImageError{
                            "No generated image URL in result"}),
                        IndigoApiStatus::kInvalidResponse);
      }
      const GURL url(*url_string);
      if (!url.is_valid()) {
        return complete(base::unexpected(GenerateImageError{
                            "Invalid generated image URL: " + *url_string}),
                        IndigoApiStatus::kInvalidResponse);
      }
      std::string mime_type;
      std::string charset;
      if (!net::DataURL::Parse(url, &mime_type, &charset, nullptr)) {
        return complete(base::unexpected(GenerateImageError{"Not a data URL: " +
                                                            *url_string}),
                        IndigoApiStatus::kInvalidResponse);
      }
      if (!blink::IsSupportedImageMimeType(mime_type)) {
        return complete(base::unexpected(GenerateImageError{
                            "Unsupported image MIME type: " + mime_type}),
                        IndigoApiStatus::kInvalidResponse);
      }
      return complete(GeneratedImage{url}, IndigoApiStatus::kSuccess);
    }

    const auto* error = dict.FindDict("error");
    if (error) {
      const std::string* code = error->FindString("code");
      const std::string* message = error->FindString("message");
      std::string error_msg = "API returned error: ";
      error_msg += code ? *code : "(no error code)";
      if (message) {
        base::StrAppend(&error_msg, {" ", *message});
      }
      return complete(
          base::unexpected(GenerateImageError{std::move(error_msg)}),
          IndigoApiStatus::kApiError);
    }

    return complete(
        base::unexpected(GenerateImageError{
            "Missing result or error in response from " + url_.spec()}),
        IndigoApiStatus::kInvalidResponse);
  }

  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kGenerate, code);
    RecordApiStatusAndLatency(IndigoApiEndpoint::kGenerate,
                              MapApiErrorCodeToStatus(code),
                              base::TimeTicks::Now() - start_time_);
    std::move(callback_).Run(base::unexpected(GenerateImageError{base::StrCat(
        {"Premature failure: ", google_apis::ApiErrorCodeToString(code)})}));
  }

 private:
  GURL url_;
  std::string request_content_;
  ApiClient::GenerateCallback callback_;
  base::TimeTicks start_time_;
};

class GetStatusRequest : public google_apis::UrlFetchRequestBase {
 public:
  GetStatusRequest(google_apis::RequestSender* sender,
                   GURL url,
                   ApiClient::StatusCallback callback)
      : google_apis::UrlFetchRequestBase(sender,
                                         google_apis::ProgressCallback(),
                                         google_apis::ProgressCallback()),
        url_(url),
        callback_(std::move(callback)),
        start_time_(base::TimeTicks::Now()) {}

  GetStatusRequest(const GetStatusRequest&) = delete;
  GetStatusRequest& operator=(const GetStatusRequest&) = delete;
  ~GetStatusRequest() override {
    if (callback_) {
      RecordApiStatusAndLatency(IndigoApiEndpoint::kGetStatus,
                                IndigoApiStatus::kCancelled,
                                base::TimeTicks::Now() - start_time_);
      std::move(callback_).Run(
          base::unexpected(StatusError{"Request cancelled"}));
    }
  }

 protected:
  GURL GetURL() const override { return url_; }

  google_apis::HttpRequestMethod GetRequestType() const override {
    return google_apis::HttpRequestMethod::kPost;
  }

  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override {
    *upload_content_type = "application/json";
    *upload_content = R"({"fetchAccountEligibility":true})";
    return true;
  }

  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override {
    return code;
  }

  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override {
    return error == google_apis::HTTP_SUCCESS;
  }

  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kGetStatus, GetErrorCode());

    auto complete = [this](base::expected<StatusResult, StatusError> result,
                           IndigoApiStatus status) {
      RecordApiStatusAndLatency(IndigoApiEndpoint::kGetStatus, status,
                                base::TimeTicks::Now() - start_time_);
      std::move(callback_).Run(std::move(result));
      OnProcessURLFetchResultsComplete();
    };

    if (GetErrorCode() != google_apis::HTTP_SUCCESS) {
      return complete(base::unexpected(StatusError{
                          GetErrorPrefix(GetErrorCode()) +
                          google_apis::ApiErrorCodeToString(GetErrorCode())}),
                      MapApiErrorCodeToStatus(GetErrorCode()));
    }

    auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
        response_body, base::JSON_PARSE_RFC);
    if (!parse_result.has_value()) {
      return complete(base::unexpected(StatusError{
                          "Invalid JSON response from " + url_.spec() + ": " +
                          parse_result.error().ToString()}),
                      IndigoApiStatus::kParseError);
    }
    if (!parse_result->is_dict()) {
      return complete(
          base::unexpected(StatusError{"Invalid JSON response from " +
                                       url_.spec() + ": not a dictionary"}),
          IndigoApiStatus::kInvalidResponse);
    }

    const auto& dict = parse_result->GetDict();

    const auto* error = dict.FindDict("error");
    if (error) {
      const std::string* message = error->FindString("message");
      return complete(
          base::unexpected(StatusError{
              message ? *message : "API returned error without message"}),
          IndigoApiStatus::kApiError);
    }

    std::optional<bool> has_user_image = dict.FindBool("hasUserImage");
    std::optional<bool> account_eligible_for_try_on =
        dict.FindBool("accountEligibleForTryOn");
    return complete(
        StatusResult{.has_user_image = has_user_image.value_or(false),
                     .is_service_supported_for_account =
                         account_eligible_for_try_on.value_or(false)},
        IndigoApiStatus::kSuccess);
  }

  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kGetStatus, code);
    RecordApiStatusAndLatency(IndigoApiEndpoint::kGetStatus,
                              MapApiErrorCodeToStatus(code),
                              base::TimeTicks::Now() - start_time_);
    std::move(callback_).Run(base::unexpected(StatusError{base::StrCat(
        {"Premature failure: ", google_apis::ApiErrorCodeToString(code)})}));
  }

 private:
  GURL url_;
  ApiClient::StatusCallback callback_;
  base::TimeTicks start_time_;
};

class DeleteRequest : public google_apis::UrlFetchRequestBase {
 public:
  DeleteRequest(google_apis::RequestSender* sender,
                GURL url,
                ApiClient::DeleteCallback callback)
      : google_apis::UrlFetchRequestBase(sender,
                                         google_apis::ProgressCallback(),
                                         google_apis::ProgressCallback()),
        url_(url),
        callback_(std::move(callback)),
        start_time_(base::TimeTicks::Now()) {}

  DeleteRequest(const DeleteRequest&) = delete;
  DeleteRequest& operator=(const DeleteRequest&) = delete;
  ~DeleteRequest() override {
    if (callback_) {
      RecordApiStatusAndLatency(IndigoApiEndpoint::kDelete,
                                IndigoApiStatus::kCancelled,
                                base::TimeTicks::Now() - start_time_);
      std::move(callback_).Run(
          base::unexpected(DeleteError{"Request cancelled"}));
    }
  }

 protected:
  GURL GetURL() const override { return url_; }

  google_apis::HttpRequestMethod GetRequestType() const override {
    return google_apis::HttpRequestMethod::kPost;
  }

  bool GetContentData(std::string* upload_content_type,
                      std::string* upload_content) override {
    *upload_content_type = "application/json";
    *upload_content = "{}";
    return true;
  }

  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override {
    return code;
  }

  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override {
    return error == google_apis::HTTP_SUCCESS;
  }

  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kDelete, GetErrorCode());

    auto complete = [this](base::expected<void, DeleteError> result,
                           IndigoApiStatus status) {
      RecordApiStatusAndLatency(IndigoApiEndpoint::kDelete, status,
                                base::TimeTicks::Now() - start_time_);
      std::move(callback_).Run(std::move(result));
      OnProcessURLFetchResultsComplete();
    };

    if (GetErrorCode() != google_apis::HTTP_SUCCESS) {
      return complete(base::unexpected(DeleteError{
                          GetErrorPrefix(GetErrorCode()) +
                          google_apis::ApiErrorCodeToString(GetErrorCode())}),
                      MapApiErrorCodeToStatus(GetErrorCode()));
    }

    if (response_body.empty()) {
      return complete(base::unexpected(DeleteError{
                          "Unexpected empty response from " + url_.spec()}),
                      IndigoApiStatus::kInvalidResponse);
    }

    auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
        response_body, base::JSON_PARSE_RFC);
    if (!parse_result.has_value()) {
      return complete(base::unexpected(DeleteError{
                          "Invalid JSON response from " + url_.spec() + ": " +
                          parse_result.error().ToString()}),
                      IndigoApiStatus::kParseError);
    }
    if (!parse_result->is_dict()) {
      return complete(
          base::unexpected(DeleteError{"Invalid JSON response from " +
                                       url_.spec() + ": not a dictionary"}),
          IndigoApiStatus::kInvalidResponse);
    }

    const auto& dict = parse_result->GetDict();
    const auto* error = dict.FindDict("error");
    if (error) {
      const std::string* message = error->FindString("message");
      return complete(
          base::unexpected(DeleteError{
              message ? *message : "API returned error without message"}),
          IndigoApiStatus::kApiError);
    }

    if (dict.empty()) {
      return complete(base::ok(), IndigoApiStatus::kSuccess);
    }

    return complete(
        base::unexpected(DeleteError{
            "Unexpected non-empty JSON response from " + url_.spec()}),
        IndigoApiStatus::kInvalidResponse);
  }

  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override {
    RecordApiHttpResponse(IndigoApiEndpoint::kDelete, code);
    RecordApiStatusAndLatency(IndigoApiEndpoint::kDelete,
                              MapApiErrorCodeToStatus(code),
                              base::TimeTicks::Now() - start_time_);
    std::move(callback_).Run(base::unexpected(DeleteError{base::StrCat(
        {"Premature failure: ", google_apis::ApiErrorCodeToString(code)})}));
  }

 private:
  GURL url_;
  ApiClient::DeleteCallback callback_;
  base::TimeTicks start_time_;
};

}  // namespace

ApiClient::ApiClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      generate_url_(features::kIndigoGenerateUrl.Get()),
      status_url_(features::kIndigoStatusUrl.Get()),
      delete_url_(features::kIndigoDeleteUrl.Get()) {
  DCHECK(identity_manager);
  DCHECK(url_loader_factory);

  identity_manager_observation_.Observe(identity_manager_);
  ReconstructRequestSender();
}

ApiClient::~ApiClient() = default;

void ApiClient::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  switch (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin)) {
    case signin::PrimaryAccountChangeEvent::Type::kSet:
    case signin::PrimaryAccountChangeEvent::Type::kCleared:
      ReconstructRequestSender();
      break;
    case signin::PrimaryAccountChangeEvent::Type::kNone:
      break;
  }
}

void ApiClient::ReconstructRequestSender() {
  auto account_id =
      identity_manager_->GetPrimaryAccountId(signin::ConsentLevel::kSignin);
  if (account_id.empty()) {
    request_sender_.reset();
    return;
  }

  request_sender_ = std::make_unique<google_apis::RequestSender>(
      std::make_unique<google_apis::AuthService>(
          identity_manager_, account_id, url_loader_factory_,
          signin::OAuthConsumerId::kIndigo),
      url_loader_factory_,
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})
          .get(),
      /*custom_user_agent=*/std::string(), MISSING_TRAFFIC_ANNOTATION);
}

base::OnceClosure ApiClient::Generate(
    base::span<const uint8_t> product_image_bytes,
    GenerateCallback callback) {
  if (product_image_bytes.size() > 4 * 1024 * 1024) {
    base::UmaHistogramEnumeration("Indigo.Api.Generate.Status",
                                  IndigoApiStatus::kTooLarge);
    std::move(callback).Run(base::unexpected(
        GenerateImageError{"Product image is too large (> 4MB)"}));
    return base::NullCallback();
  }

  if (!request_sender_) {
    base::UmaHistogramEnumeration("Indigo.Api.Generate.Status",
                                  IndigoApiStatus::kNoSignedInUser);
    std::move(callback).Run(
        base::unexpected(GenerateImageError{"No signed in user"}));
    return base::NullCallback();
  }

  if (!generate_url_.is_valid()) {
    base::UmaHistogramEnumeration("Indigo.Api.Generate.Status",
                                  IndigoApiStatus::kInvalidUrl);
    std::move(callback).Run(base::unexpected(GenerateImageError{
        base::StrCat({"Invalid generate URL: ", generate_url_.spec()})}));
    return base::NullCallback();
  }

  return request_sender_->StartRequestWithAuthRetry(
      std::make_unique<GenerateRequest>(request_sender_.get(), generate_url_,
                                        product_image_bytes,
                                        std::move(callback)));
}

void ApiClient::GetStatus(StatusCallback callback) {
  if (!request_sender_) {
    base::UmaHistogramEnumeration("Indigo.Api.GetStatus.Status",
                                  IndigoApiStatus::kNoSignedInUser);
    std::move(callback).Run(base::unexpected(StatusError{"No signed in user"}));
    return;
  }

  if (!status_url_.is_valid()) {
    base::UmaHistogramEnumeration("Indigo.Api.GetStatus.Status",
                                  IndigoApiStatus::kInvalidUrl);
    std::move(callback).Run(base::unexpected(StatusError{
        base::StrCat({"Invalid status URL: ", status_url_.spec()})}));
    return;
  }

  request_sender_->StartRequestWithAuthRetry(std::make_unique<GetStatusRequest>(
      request_sender_.get(), status_url_, std::move(callback)));
}

void ApiClient::Delete(DeleteCallback callback) {
  if (!request_sender_) {
    base::UmaHistogramEnumeration("Indigo.Api.Delete.Status",
                                  IndigoApiStatus::kNoSignedInUser);
    std::move(callback).Run(base::unexpected(DeleteError{"No signed in user"}));
    return;
  }

  if (!delete_url_.is_valid()) {
    base::UmaHistogramEnumeration("Indigo.Api.Delete.Status",
                                  IndigoApiStatus::kInvalidUrl);
    std::move(callback).Run(base::unexpected(DeleteError{
        base::StrCat({"Invalid delete URL: ", delete_url_.spec()})}));
    return;
  }

  request_sender_->StartRequestWithAuthRetry(std::make_unique<DeleteRequest>(
      request_sender_.get(), delete_url_, std::move(callback)));
}

}  // namespace indigo
