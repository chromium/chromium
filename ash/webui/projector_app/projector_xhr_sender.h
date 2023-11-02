// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_
#define ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_

#include <map>
#include <string>

#include "ash/webui/projector_app/projector_oauth_token_fetcher.h"
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
constexpr char kRequestMethodPatch[] = "PATCH";

/**
 * Projector XHR sender. Used by Projector App to send XHR requests.
 */
class ProjectorXhrSender {
 public:
  // Callback triggered when a XHR request is completed. `response_body`
  // contains the response text if success, empty otherwise. `error` contains
  // error message if not success, empty otherwise.
  using SendRequestCallback =
      base::OnceCallback<void(bool success,
                              const std::string& response_body,
                              const std::string& error)>;

  explicit ProjectorXhrSender(
      network::mojom::URLLoaderFactory* url_loader_factory);
  ProjectorXhrSender(const ProjectorXhrSender&) = delete;
  ProjectorXhrSender& operator=(const ProjectorXhrSender&) = delete;
  virtual ~ProjectorXhrSender();

  // Send XHR request and trigger the callback when complete.
  // This will attempt to fetch OAuth token for the provided email address
  // and fallback to primary account email if account_email is not provided
  // or ProjectorViewerUseSecondaryAccount flag is disabled.
  virtual void Send(const GURL& url,
                    const std::string& method,
                    const std::string& request_body,
                    bool use_credentials,
                    bool use_api_key,
                    SendRequestCallback callback,
                    const base::Value::Dict& headers = base::Value::Dict(),
                    const std::string& account_email = std::string());

 private:
  // Triggered when an OAuth token fetch completed.
  void OnAccessTokenRequestCompleted(const GURL& url,
                                     const std::string& method,
                                     const std::string& request_body,
                                     const base::Value::Dict& headers,
                                     SendRequestCallback callback,
                                     const std::string& email,
                                     GoogleServiceAuthError error,
                                     const signin::AccessTokenInfo& info);

  void SendRequest(const GURL& url,
                   const std::string& method,
                   const std::string& request_body,
                   const std::string& token,
                   const base::Value::Dict& headers,
                   SendRequestCallback callback);

  // Triggered when an XHR request completed.
  void OnSimpleURLLoaderComplete(int request_id,
                                 SendRequestCallback callback,
                                 std::unique_ptr<std::string> response_body);

  // Validate the email address provided with xhr request
  bool IsValidEmail(const std::string& email);

  ProjectorOAuthTokenFetcher oauth_token_fetcher_;
  network::mojom::URLLoaderFactory* url_loader_factory_ = nullptr;

  // Next request ID.
  int next_request_id_ = 0;
  // The map to hold the SimpleURLLoader for each request. The key is a unique
  // request ID associated with each request.
  std::map<int, std::unique_ptr<network::SimpleURLLoader>> loader_map_;

  base::WeakPtrFactory<ProjectorXhrSender> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WEBUI_PROJECTOR_APP_PROJECTOR_XHR_SENDER_H_
