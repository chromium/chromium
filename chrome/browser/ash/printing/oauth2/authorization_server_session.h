// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_SESSION_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_SESSION_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

// Helper function that parse scope field (a list of names).
base::flat_set<std::string> ParseScope(const std::string& scope);

// This class represents single OAuth2 session and is responsible for acquiring
// and refreshing the access token.
class AuthorizationServerSession {
 public:
  // Constructor.
  AuthorizationServerSession(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& token_endpoint_uri,
      base::flat_set<std::string>&& scope);
  // Not copyable.
  AuthorizationServerSession(const AuthorizationServerSession&) = delete;
  AuthorizationServerSession& operator=(const AuthorizationServerSession&) =
      delete;
  // Destructor.
  ~AuthorizationServerSession();

  // Returns the access token or an empty string if the access token is not
  // known yet.
  const std::string& access_token() const { return access_token_; }

  // Returns true <=> the scope contains all elements from `scope`.
  bool ContainsAll(const base::flat_set<std::string>& scope) const;

  // Adds `callback` to the end of the waiting list. These callbacks are not
  // called internally. Instead, they can be retrieved with TakeWaitingList().
  void AddToWaitingList(StatusCallback callback);
  // Returns the waiting list by moving it. After calling this method
  // the waiting list is empty.
  std::vector<StatusCallback> TakeWaitingList();

  // Prepares and sends First Token Request. Results are returned by `callback`.
  // If the request is successful, the callback returns StatusCode::kOK and the
  // access token as the second parameter. Otherwise, the error code with
  // a message is returned.
  void SendFirstTokenRequest(const std::string& client_id,
                             const std::string& authorization_code,
                             const std::string& code_verifier,
                             StatusCallback callback);

  // Resets the current access token to empty string and sends Next Token
  // Request to obtain a new one. If the request is successful, the callback
  // returns StatusCode::kOK and the access token as the second parameter.
  // If the server does not allow to refresh the access token or the refresh
  // token expired, the status StatusCode::kAuthorizationNeeded is returned.
  // Otherwise, the error code with a message is returned.
  void SendNextTokenRequest(StatusCallback callback);

 private:
  // Analyzes response for First Token Request.
  void OnFirstTokenResponse(StatusCallback callback, StatusCode status);

  // Analyzes response for Next Token Request.
  void OnNextTokenResponse(StatusCallback callback, StatusCode status);

  // URL of the endpoint at the Authorization Server.
  const GURL token_endpoint_uri_;

  // Set of scopes requested by the client and/or granted by the
  // Authorization Server.
  base::flat_set<std::string> scope_;

  // Access token of the current OAuth2 session.
  std::string access_token_;
  // Refresh token of the current OAuth2 session.
  std::string refresh_token_;

  // The waiting list - a vector of waiting callbacks.
  std::vector<StatusCallback> callbacks_;

  // The object used for communication with the Authorization Server.
  HttpExchange http_exchange_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_SESSION_H_
