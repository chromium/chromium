// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_user_status_request.h"

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "google_apis/common/api_error_codes.h"

namespace {
constexpr char kIsGlicEnabled[] = "isGlicEnabled";
constexpr char kIsAccessDeniedByAdmin[] = "isAccessDeniedByAdmin";
}  // namespace

namespace glic {
GlicUserStatusRequest::GlicUserStatusRequest(
    google_apis::RequestSender* sender,
    GURL url,
    base::OnceCallback<void(UserStatusCode result_code)>
        process_response_callback)
    : UrlFetchRequestBase(sender,
                          google_apis::ProgressCallback(),
                          google_apis::ProgressCallback()),
      url_(url),
      process_response_callback_(std::move(process_response_callback)) {}

GlicUserStatusRequest::~GlicUserStatusRequest() = default;

GURL GlicUserStatusRequest::GetURL() const {
  return url_;
}

google_apis::ApiErrorCode GlicUserStatusRequest::MapReasonToError(
    google_apis::ApiErrorCode code,
    const std::string& reason) {
  // This method is to map error reason parsed from response body to
  // ApiErrorCode. we assume for now that result is to be sent as ApiErrorCode.
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
  std::move(process_response_callback_)
      .Run(MapApiErrorCodeAndResponseBodyToUserStatus(GetErrorCode(),
                                                      response_body));

  OnProcessURLFetchResultsComplete();
}

// called when request is canceled or auth is failed.
void GlicUserStatusRequest::RunCallbackOnPrematureFailure(
    google_apis::ApiErrorCode error) {
  std::move(process_response_callback_)
      .Run(MapApiErrorCodeAndResponseBodyToUserStatus(error, ""));
}

UserStatusCode
GlicUserStatusRequest::MapApiErrorCodeAndResponseBodyToUserStatus(
    google_apis::ApiErrorCode api_error_code,
    std::string_view response_body) {
  if (api_error_code != google_apis::HTTP_SUCCESS) {
    return UserStatusCode::SERVER_UNAVAILABLE;
  }

  // Parse response body to JSON in the form of
  // {
  //    is_glic_enabled: true/false
  //    is_access_denied_by_admin: true/false
  // }
  std::optional<base::Value::Dict> parsed =
      base::JSONReader::ReadDict(response_body);

  if (!parsed.has_value()) {
    DVLOG(1) << "Failed reading response body: " << response_body;
    return UserStatusCode::SERVER_UNAVAILABLE;
  }

  // The feature is enabled (if the response fails to mention it, we assume it
  // is).
  if (parsed->FindBool(kIsGlicEnabled).value_or(true)) {
    return UserStatusCode::ENABLED;
  }

  // The feature is disabled (find the reason, if given).
  return parsed->FindBool(kIsAccessDeniedByAdmin).value_or(false)
             ? UserStatusCode::DISABLED_BY_ADMIN
             : UserStatusCode::DISABLED_OTHER;
}
}  // namespace glic
