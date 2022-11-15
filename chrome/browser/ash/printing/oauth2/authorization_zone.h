// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

class ClientIdsDatabase;

// The class AuthorizationZone is responsible for handling sessions with single
// Authorization Server. It creates and maintains OAuth2 sessions.
//
// How to use:
//  * InitAuthorization(...) - the callback returns URL that must be open in an
//    internet browser to allow a user to go through an authorization procedure.
//  * FinishAuthorization(...) - this must be called when the authorization
//    procedure run in the internet browser is completed.
//  * GetEndpointAccessToken(...) - the callback returns endpoint access token
//    for given IPP Endpoint. You can call this method as the first one and
//    then fallback to InitAuthorization(...)/FinishAuthorization(...) when
//    it returns the status StatusCode::kAuthorizationNeeded. This method can
//    be called repeatedly and will return the same token for the same
//    parameters until the method from the next point is called.
//  * MarkEndpointAccessTokenAsExpired(...) - call this method to mark the
//    endpoint access token as expired. Then the following call to
//    GetEndpointAccessToken(...) should obtain a new endpoint access token or
//    returns StatusCode::kAuthorizationNeeded when a new authorization
//    procedure is needed (i.e. a new call to InitAuthorization(...) and
//    FinishAuthorization(...) is required).
//
class AuthorizationZone {
 public:
  // `client_ids_database` cannot be nullptr and must outlive created object.
  static std::unique_ptr<AuthorizationZone> Create(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& authorization_server_uri,
      ClientIdsDatabase* client_ids_database);

  AuthorizationZone(const AuthorizationZone&) = delete;
  AuthorizationZone& operator=(const AuthorizationZone&) = delete;
  virtual ~AuthorizationZone() = default;

  // Starts authorization process. If successful, the `callback` is returned
  // with StatusCode::kOK and with an authorization URL that must be open in an
  // internet browser. Before calling this method the caller should make sure
  // that creating a new session is necessary by calling the method
  // GetEndpointAccessToken(...) first.
  virtual void InitAuthorization(const std::string& scope,
                                 StatusCallback callback) = 0;
  // Finalizes authorization process. As an parameter this method takes an URL
  // that the internet browser was redirected to at the end of the authorization
  // procedure completed by the user. The return code StatusCode::kOK means
  // that a new OAuth2 session was created and the caller can now use the method
  // GetEndpointAccessToken(...) to get an endpoint access token.
  virtual void FinishAuthorization(const GURL& redirect_url,
                                   StatusCallback callback) = 0;
  // Obtains an endpoint access token to use with the given IPP Endpoint. If
  // succeeded `callback` returns StatusCode::kOK and Endpoint Access Token as
  // `data`. StatusCode::kAuthorizationNeeded means that the caller must call
  // methods InitAuthorization(...) and FinishAuthorization(...) first to open
  // OAuth2 session. The parameter `scope` is used only when an endpoint access
  // token for the given IPP endpoint does not exists yet (the parameter has no
  // effectes when the endpoint access token already exists).
  virtual void GetEndpointAccessToken(const chromeos::Uri& ipp_endpoint,
                                      const std::string& scope,
                                      StatusCallback callback) = 0;
  // This method must be called when given `endpoint_access_token` stops
  // working. It marks the endpoint access token as expired. The next call to
  // GetEndpointAccessToken(...) will start internally a procedure to obtain a
  // new endpoint access token.
  virtual void MarkEndpointAccessTokenAsExpired(
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) = 0;
  // This method must be called when the Authorization Zone becomes untrusted.
  // The method cancels all existing sessions and calls all pending callbacks
  // in the object with status StatusCode::kUntrustedAuthorizationServer.
  virtual void MarkAuthorizationZoneAsUntrusted() = 0;

 protected:
  AuthorizationZone() = default;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_H_
