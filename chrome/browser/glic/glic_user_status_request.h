// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_USER_STATUS_REQUEST_H_
#define CHROME_BROWSER_GLIC_GLIC_USER_STATUS_REQUEST_H_

#include "base/functional/callback.h"
#include "chrome/browser/glic/glic_user_status_code.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/common/request_sender.h"

namespace glic {

class GlicUserStatusRequest : public google_apis::UrlFetchRequestBase {
 public:
  explicit GlicUserStatusRequest(
      google_apis::RequestSender* sender,
      GURL url,

      base::OnceCallback<void(UserStatusCode result_code)>
          process_response_callback);
  GlicUserStatusRequest(const GlicUserStatusRequest&) = delete;
  GlicUserStatusRequest& operator=(const GlicUserStatusRequest&) = delete;
  ~GlicUserStatusRequest() override;

 protected:
  GURL GetURL() const override;

  google_apis::ApiErrorCode MapReasonToError(
      google_apis::ApiErrorCode code,
      const std::string& reason) override;

  bool IsSuccessfulErrorCode(google_apis::ApiErrorCode error) override;

  void ProcessURLFetchResults(
      const network::mojom::URLResponseHead* response_head,
      base::FilePath response_file,
      std::string response_body) override;

  void RunCallbackOnPrematureFailure(google_apis::ApiErrorCode code) override;

 private:
  static UserStatusCode MapApiErrorCodeAndResponseBodyToUserStatus(
      google_apis::ApiErrorCode result_code,
      std::string_view response_body);

  GURL url_;
  base::OnceCallback<void(UserStatusCode result_code)>
      process_response_callback_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_USER_STATUS_REQUEST_H_
