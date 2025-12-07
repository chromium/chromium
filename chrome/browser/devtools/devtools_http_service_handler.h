// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_HANDLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_HANDLER_H_

#include <compare>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chrome/browser/devtools/devtools_dispatch_http_request_params.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/simple_url_loader.h"

class GURL;
class Profile;

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

class DevToolsHttpServiceHandler {
 public:
  struct Result {
    enum class Error {
      kNone,              // Success
      kServiceNotFound,   // The service is not registered.
      kAccessDenied,      // The path/method is not in the allow-list.
      kValidationFailed,  // Service-specific validation failed.
      kTokenFetchFailed,  // Could not get an OAuth token.
      kNetworkError,      // A network-level error occurred (e.g., DNS failure).
      kHttpError,         // Server returned a non-2xx HTTP status.
    };

    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);

    Error error = Error::kNone;
    int net_error = 0;
    int http_status = -1;
    std::optional<std::string> response_body;
    std::string error_detail;
  };

  using Callback = base::OnceCallback<void(std::unique_ptr<Result> result)>;

  DevToolsHttpServiceHandler();
  virtual ~DevToolsHttpServiceHandler();

  void Request(Profile* profile,
               const DevToolsDispatchHttpRequestParams& params,
               Callback callback);

 protected:
  // Performs service-specific pre-request validation. Can be asynchronous.
  virtual void CanMakeRequest(Profile* profile,
                              base::OnceCallback<void(bool success)> callback);

 private:
  // Returns the base URL for the service's API.
  virtual GURL BaseURL() const = 0;

  // Returns the OAuth scopes required for authenticating with the service.
  virtual signin::ScopeSet OAuthScopes() const = 0;

  // Returns the traffic annotation for the request.
  virtual net::NetworkTrafficAnnotationTag NetworkTrafficAnnotationTag()
      const = 0;

  void OnValidationDone(Callback callback,
                        Profile* profile,
                        const DevToolsDispatchHttpRequestParams& params,
                        bool validation_success);

  void OnTokenFetched(Callback callback,
                      Profile* profile,
                      const DevToolsDispatchHttpRequestParams& params,
                      base::UnguessableToken fetcher_id,
                      GoogleServiceAuthError error,
                      signin::AccessTokenInfo access_token_info);

  void OnRequestComplete(
      Callback callback,
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      std::optional<std::string> response_body);

  std::map<base::UnguessableToken, std::unique_ptr<signin::AccessTokenFetcher>>
      access_token_fetchers_;

  base::WeakPtrFactory<DevToolsHttpServiceHandler> weak_factory_{this};
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_HTTP_SERVICE_HANDLER_H_
