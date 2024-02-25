// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_

#include <map>
#include <string>

#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
#include "ash/webui/projector_app/public/mojom/projector_types.mojom.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace base {
class GURL;
}  // namespace base

namespace network {
class SimpleURLLoader;

namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace ash {

constexpr char kDriveV3BaseUrl[] = "https://www.googleapis.com/drive/v3/files/";

/**
 * Projector XHR sender. Used by Projector App to send XHR requests.
 */
class ProjectorXhrSender {
 public:
  // Callback triggered when a XHR request is completed. `response_body`
  // contains the response text if success, empty otherwise. `response_code`
  // contains the response code.
  using SendRequestCallback =
      base::OnceCallback<void(projector::mojom::XhrResponsePtr)>;

  explicit ProjectorXhrSender(
      network::mojom::URLLoaderFactory* url_loader_factory);
  ProjectorXhrSender(const ProjectorXhrSender&) = delete;
  ProjectorXhrSender& operator=(const ProjectorXhrSender&) = delete;
  virtual ~ProjectorXhrSender();

  // Send XHR request and trigger the callback when complete.
  // There are a few credentials could be used for authorizing a request:
  // 1. API Key: when `use_api_key` is true, it is a anonymous request. End
  // user credentials will be ignored.
  // 2. Account email: By default, primary account email is used to get OAuth
  // token for authorizing the request. If `account_email` is specified, it
  // will be used instead.
  // TODO(b/288457397): Clean up so that account email is required when sending
  // request with OAuth token.
  // 3. Credentials: when `use_credentials` is true, the request will be sent
  // with cookie alongs with auth token. One use case is allowing
  // get_video_info response to add streaming auth token in cookie. There is
  // no use case for sending requests with credentials only (without oauth
  // token).
  virtual void Send(
      const GURL& url,
      projector::mojom::RequestType method,
      const std::optional<std::string>& request_body,
      bool use_credentials,
      bool use_api_key,
      SendRequestCallback callback,
      const std::optional<base::flat_map<std::string, std::string>>& headers =
          std::nullopt,
      const std::optional<std::string>& account_email = std::nullopt);

 private:
  // Triggered when an OAuth token fetch completed.
  void OnAccessTokenRequestCompleted(
      const GURL& url,
      projector::mojom::RequestType method,
      const std::optional<std::string>& request_body,
      const std::optional<base::flat_map<std::string, std::string>>& headers,
      bool use_credentials,
      SendRequestCallback callback,
      const std::string& email,
      GoogleServiceAuthError error,
      const signin::AccessTokenInfo& info);

  void SendRequest(
      const GURL& url,
      projector::mojom::RequestType method,
      const std::optional<std::string>& request_body,
      const std::string& token,
      const std::optional<base::flat_map<std::string, std::string>>& headers,
      bool allow_cookie,
      SendRequestCallback callback);

  // Triggered when an XHR request completed.
  void OnSimpleURLLoaderComplete(int request_id,
                                 SendRequestCallback callback,
                                 const std::string& token,
                                 std::unique_ptr<std::string> response_body);

  // Validate the email address provided with xhr request
  bool IsValidEmail(const std::optional<std::string>& email_check);

  ProjectorOAuthTokenFetcher oauth_token_fetcher_;
  raw_ptr<network::mojom::URLLoaderFactory> url_loader_factory_ = nullptr;

  // Next request ID.
  int next_request_id_ = 0;
  // The map to hold the SimpleURLLoader for each request. The key is a unique
  // request ID associated with each request.
  std::map<int, std::unique_ptr<network::SimpleURLLoader>> loader_map_;

  base::WeakPtrFactory<ProjectorXhrSender> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_
