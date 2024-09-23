// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

class GURL;
class Profile;

namespace chromeos {
class Uri;
}  // namespace chromeos

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace ash::printing::oauth2 {

class AuthorizationZone;
class ClientIdsDatabase;

// This class is responsible for managing OAuth2 sessions required to get access
// to some printers. In the API provided by the class, printers are referred to
// as IPP Endpoints. IPP Endpoints that require OAuth2 token report the
// following IPP attributes in the response for Get-Printer-Attributes request:
//   * oauth-authorization-server-uri - the URL of the Authorization Server;
//   * oauth-authorization-scope - optional, if missing use an empty string.
// These two values correspond to the parameters `auth_server` and `scope` in
// the API below.
//
// How to use:
//  * SaveAuthorizationServerAsTrusted() - this must be called once for each
//    Authorization Server to mark it as trusted. The list of trusted
//    Authorization Servers is saved in user's profile. All API calls for any
//    Authorization Server not included in the trusted list will fail with the
//    error StatusCode::kUntrustedAuthorizationServer.
//  * InitAuthorization() - the callback returns a URL that must be opened in an
//    internet browser to allow a user to go through an authorization procedure.
//  * FinishAuthorization() - this method finalizes the authorization procedure
//    started by InitAuthorization(). The authorization procedure in the
//    internet browser is completed when the browser receives a response with
//    the HTTP 302 status code. The value of the "Location" attribute from the
//    HTTP header must be passed to this method to finalize the process and
//    open an OAuth2 session with the Authorization Server.
//  * GetEndpointAccessToken() - the callback returns an endpoint access token
//    for given IPP Endpoint. You can call this method as the first one and
//    then fallback to InitAuthorization()/FinishAuthorization() when it
//    returns the status StatusCode::kAuthorizationNeeded. This method can
//    be called repeatedly and will return the same token for the same
//    parameters until the method MarkEndpointAccessTokenAsExpired() is called.
//  * MarkEndpointAccessTokenAsExpired() - call this method to mark the
//    endpoint access token as expired. Then the following call to
//    GetEndpointAccessToken() will try to obtain a new endpoint access token
//    or returns StatusCode::kAuthorizationNeeded when a new authorization
//    procedure is needed (i.e. a new call to InitAuthorization() and
//    FinishAuthorization() is required).
//
// Results and errors are returned by StatusCallback passed as the last
// parameter. See chrome/browser/ash/printing/oauth2/status_code.h for more
// details.
class AuthorizationZonesManager : public KeyedService {
 public:
  using CreateAuthZoneCallback =
      base::RepeatingCallback<std::unique_ptr<AuthorizationZone>(
          const GURL& url,
          ClientIdsDatabase* client_ids_database)>;

  // `profile` must not be nullptr.
  static std::unique_ptr<AuthorizationZonesManager> Create(Profile* profile);
  static std::unique_ptr<AuthorizationZonesManager> CreateForTesting(
      Profile* profile,
      CreateAuthZoneCallback auth_zone_creator,
      std::unique_ptr<ClientIdsDatabase> client_ids_database,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory);

  ~AuthorizationZonesManager() override;
  virtual syncer::DataTypeSyncBridge* GetDataTypeSyncBridge() = 0;

  // Marks `auth_server` as trusted.
  virtual StatusCode SaveAuthorizationServerAsTrusted(
      const GURL& auth_server) = 0;

  // Starts authorization process. If successful, the `callback` is called
  // with StatusCode::kOK and with an authorization URL that must be opened in
  // an internet browser to enable the user to complete the authorization
  // process. Before calling this method the caller should make sure that
  // creating a new session is necessary by calling the method
  // GetEndpointAccessToken() first.
  virtual void InitAuthorization(const GURL& auth_server,
                                 const std::string& scope,
                                 StatusCallback callback) = 0;

  // Finalizes authorization process. As an parameter this method takes an URL
  // that the internet browser was redirected to at the end of the authorization
  // procedure completed by the user. The return code StatusCode::kOK means
  // that a new OAuth2 session was created and the caller can now use the method
  // GetEndpointAccessToken() to get an endpoint access token.
  virtual void FinishAuthorization(const GURL& auth_server,
                                   const GURL& redirect_url,
                                   StatusCallback callback) = 0;

  // Obtains an endpoint access token to use with the given IPP Endpoint. If
  // succeeded `callback` returns StatusCode::kOK and endpoint access token as
  // `data`. StatusCode::kAuthorizationNeeded means that the caller must call
  // methods InitAuthorization() and FinishAuthorization() first to open OAuth2
  // session. The parameter `scope` is used only when an endpoint access token
  // for the given IPP endpoint does not exists yet (the parameter has no
  // effects when the endpoint access token already exists).
  virtual void GetEndpointAccessToken(const GURL& auth_server,
                                      const chromeos::Uri& ipp_endpoint,
                                      const std::string& scope,
                                      StatusCallback callback) = 0;

  // This method marks the `endpoint_access_token` issued for `ipp_endpoint` as
  // expired. The next call to GetEndpointAccessToken() will start internally a
  // procedure to obtain a new endpoint access token. This method should be
  // called when the IPP Endpoint rejects a request with `endpoint_access_token`
  // by sending back a response with the HTTP 401 status code. If the HTTP
  // header of that response contains "error" attribute, you should check if it
  // equals "invalid_token" before calling this method. See RFC 6750 for more
  // details.
  virtual void MarkEndpointAccessTokenAsExpired(
      const GURL& auth_server,
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) = 0;

 protected:
  AuthorizationZonesManager();
};

}  // namespace ash::printing::oauth2

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONES_MANAGER_H_
