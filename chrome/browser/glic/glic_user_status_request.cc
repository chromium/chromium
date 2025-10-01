// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_user_status_request.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "components/variations/variations_ids_provider.h"
#include "google_apis/common/api_error_codes.h"

namespace {
// The server response would be first converted to JSON. The JSON object would
// be converted to this struct.
struct GlicUserStatusResponse {
  bool is_glic_enabled = true;
  bool is_access_denied_by_admin = false;
  bool is_enterprise_account_data_protected = false;
  static void RegisterJSONConverter(
      base::JSONValueConverter<GlicUserStatusResponse>* converter) {
    converter->RegisterBoolField(glic::kIsGlicEnabled,
                                 &GlicUserStatusResponse::is_glic_enabled);
    converter->RegisterBoolField(
        glic::kIsAccessDeniedByAdmin,
        &GlicUserStatusResponse::is_access_denied_by_admin);
    converter->RegisterBoolField(
        glic::kIsEnterpriseAccountDataProtected,
        &GlicUserStatusResponse::is_enterprise_account_data_protected);
  }
};
}  // namespace

namespace glic {
GlicUserStatusRequest::GlicUserStatusRequest(
    google_apis::RequestSender* sender,
    variations::VariationsClient* variations_client,
    GURL url,
    base::OnceCallback<void(const CachedUserStatus&)> process_response_callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      url_(url),
      variations_client_(variations_client),
      process_response_callback_(std::move(process_response_callback)) {}

GlicUserStatusRequest::~GlicUserStatusRequest() = default;

std::vector<std::string> GlicUserStatusRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;

  if (!variations_client_) {
    return headers;
  }

  variations::mojom::VariationsHeadersPtr variations =
      variations_client_->GetVariationsHeaders();
  if (variations_client_->IsOffTheRecord() || variations.is_null()) {
    return headers;
  }

  // The endpoint is always a Google property.
  headers.push_back(
      base::StrCat({variations::kClientDataHeader, ": ",
                    variations->headers_map.at(
                        variations::mojom::GoogleWebVisibility::FIRST_PARTY)}));

  return headers;
}

GURL GlicUserStatusRequest::GetURL() const {
  return url_;
}

google_apis::ApiErrorCode GlicUserStatusRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  // This method is to map error reason parsed from response body to
  // ApiErrorCode. we assume for now that result is to be sent as
  // ApiErrorCode.
  return code;
}

bool GlicUserStatusRequest::IsSuccessfulErrorCode(
    google_apis::ApiErrorCode error) {
  return error == google_apis::HTTP_SUCCESS;
}

void GlicUserStatusRequest::ProcessURLFetchResults(
    const network::mojom::URLResponseHead* response_head,
    base::FilePath response_file,
    std::string response_body) {
  auto cached_user_status =
      MapApiErrorCodeAndResponseBodyToUserStatus(GetErrorCode(), response_body);

  std::move(process_response_callback_).Run(cached_user_status);

  OnProcessURLFetchResultsComplete();
}

// called when request is canceled or auth is failed.
void GlicUserStatusRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  auto cached_user_status =
      MapApiErrorCodeAndResponseBodyToUserStatus(error, "");
  std::move(process_response_callback_).Run(cached_user_status);
}

CachedUserStatus
GlicUserStatusRequest::MapApiErrorCodeAndResponseBodyToUserStatus(
    google_apis::ApiErrorCode api_error_code,
    std::string_view response_body_as_string) {
  // Currently, the is_enterprise_account_data_protected is not used for any
  // Chrome behavir. Its sole use is to tell the user if their data is logged.
  // It is worse to tell the user that their data  when in fact it is, than to
  // tell the user that their data is logged when in fact it is not. (And that
  // messaging is the only thing this boolean controls). Therefore, we default
  // the field to false. If the field is to be used in other ways later, the
  // default value may need to change too.
  CachedUserStatus user_status = {
      .user_status_code = UserStatusCode::SERVER_UNAVAILABLE,
      .is_enterprise_account_data_protected = false,
      .last_updated = base::Time::Now()};

  if (api_error_code != google_apis::HTTP_SUCCESS) {
    return user_status;
  }

  // Parse response body to JSON in the form of
  // {
  //    is_glic_enabled: true/false
  //    is_access_denied_by_admin: true/false
  //    is_enterprise_account_data_protected: true/false
  // }
  std::optional<base::Value::Dict> parsed_json = base::JSONReader::ReadDict(
      response_body_as_string, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  if (!parsed_json.has_value()) {
    DVLOG(1) << "Failed reading response body: " << response_body_as_string;
    return user_status;
  }

  // Convert response body in JSON format to the GlicUserStatusResponse
  // struct.
  GlicUserStatusResponse response;
  base::JSONValueConverter<GlicUserStatusResponse> converter;

  if (!converter.Convert(parsed_json.value(), &response)) {
    return user_status;
  }

  user_status.is_enterprise_account_data_protected =
      response.is_enterprise_account_data_protected;

  if (response.is_glic_enabled) {
    // The feature is enabled (if the response fails to mention it, we assume it
    // is).
    user_status.user_status_code = UserStatusCode::ENABLED;
  } else {
    // The feature is disabled (find the reason, if given).
    user_status.user_status_code = response.is_access_denied_by_admin
                                       ? UserStatusCode::DISABLED_BY_ADMIN
                                       : UserStatusCode::DISABLED_OTHER;
  }

  return user_status;
}
}  // namespace glic
