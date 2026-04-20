// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/api_client.h"

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/auth_service.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace indigo {

namespace {

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
        callback_(std::move(callback)) {
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
    auto complete =
        [&](base::expected<GeneratedImage, GenerateImageError> result) {
          std::move(callback_).Run(std::move(result));
          OnProcessURLFetchResultsComplete();
        };

    if (GetErrorCode() != google_apis::HTTP_SUCCESS) {
      return complete(base::unexpected(GenerateImageError{
          "HTTP error: " + google_apis::ApiErrorCodeToString(GetErrorCode())}));
    }

    auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
        response_body, base::JSON_PARSE_RFC);
    if (!parse_result.has_value()) {
      return complete(base::unexpected(
          GenerateImageError{"Invalid JSON response from " + url_.spec() +
                             ": " + parse_result.error().ToString()}));
    }
    if (!parse_result->is_dict()) {
      return complete(base::unexpected(GenerateImageError{
          "Invalid JSON response from " + url_.spec() + ": not a dictionary"}));
    }

    const auto& dict = parse_result->GetDict();
    const auto* result = dict.FindDict("result");
    if (result) {
      const std::string* url_string = result->FindString("generatedImageUrl");
      if (!url_string) {
        return complete(base::unexpected(
            GenerateImageError{"No generated image URL in result"}));
      }
      const GURL url(*url_string);
      if (!url.is_valid()) {
        return complete(base::unexpected(
            GenerateImageError{"Invalid generated image URL: " + *url_string}));
      }
      return complete(GeneratedImage{url});
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
          base::unexpected(GenerateImageError{std::move(error_msg)}));
    }

    return complete(base::unexpected(GenerateImageError{
        "Missing result or error in response from " + url_.spec()}));
  }

  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override {
    std::move(callback_).Run(base::unexpected(GenerateImageError{base::StrCat(
        {"Premature failure: ", google_apis::ApiErrorCodeToString(code)})}));
  }

 private:
  GURL url_;
  std::string request_content_;
  ApiClient::GenerateCallback callback_;
};

}  // namespace

ApiClient::ApiClient(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      generate_url_(features::kIndigoGenerateUrl.Get()) {
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

void ApiClient::Generate(base::span<const uint8_t> product_image_bytes,
                         GenerateCallback callback) {
  if (!request_sender_) {
    std::move(callback).Run(
        base::unexpected(GenerateImageError{"No signed in user"}));
    return;
  }

  if (!generate_url_.is_valid()) {
    std::move(callback).Run(base::unexpected(GenerateImageError{
        base::StrCat({"Invalid generate URL: ", generate_url_.spec()})}));
    return;
  }

  request_sender_->StartRequestWithAuthRetry(std::make_unique<GenerateRequest>(
      request_sender_.get(), generate_url_, product_image_bytes,
      std::move(callback)));
}

}  // namespace indigo
