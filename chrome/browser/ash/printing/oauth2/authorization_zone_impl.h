// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_

#include <list>
#include <map>
#include <memory>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"
#include "chrome/browser/ash/printing/oauth2/authorization_zone.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

class AuthorizationServerSession;
class ClientIdsDatabase;
class IppEndpointTokenFetcher;

// The class AuthorizationZoneImpl implements functionality described in
// AuthorizationZone interface.
//
class AuthorizationZoneImpl : public AuthorizationZone {
 public:
  // `client_ids_database` cannot be nullptr and must outlive this object.
  AuthorizationZoneImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& authorization_server_uri,
      ClientIdsDatabase* client_ids_database);

  AuthorizationZoneImpl(const AuthorizationZoneImpl&) = delete;
  AuthorizationZoneImpl& operator=(const AuthorizationZoneImpl&) = delete;
  ~AuthorizationZoneImpl() override;

  // AuthorizationZone interface.
  void InitAuthorization(const std::string& scope,
                         StatusCallback callback) override;
  void FinishAuthorization(const GURL& redirect_url,
                           StatusCallback callback) override;
  void GetEndpointAccessToken(const chromeos::Uri& ipp_endpoint,
                              const std::string& scope,
                              StatusCallback callback) override;
  void MarkEndpointAccessTokenAsExpired(
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) override;
  void MarkAuthorizationZoneAsUntrusted() override;

 private:
  // This method processes (and removes) all elements from
  // `waiting_authorizations_` by initiating authorization procedures for them.
  void AuthorizationProcedure();

  // Callback for AuthorizationServerData::Initialize().
  void OnInitializeCallback(StatusCode status, std::string data);

  // Callback for AuthorizationServerSession::SendFirstTokenRequest() and
  // AuthorizationServerSession::SendNextTokenRequest().
  void OnSendTokenRequestCallback(AuthorizationServerSession* session,
                                  StatusCode status,
                                  std::string data);

  // Callback for IppEndpointTokenFetcher::SendTokenExchangeRequest(...).
  void OnTokenExchangeRequestCallback(const chromeos::Uri& ipp_endpoint,
                                      StatusCode status,
                                      std::string data);

  // Executes all callbacks from the waitlist of `ipp_endpoint`. Also, removes
  // `ipp_endpoint` when `status` != StatusCode::kOK.
  void ResultForIppEndpoint(const chromeos::Uri& ipp_endpoint,
                            StatusCode status,
                            std::string data);

  // This callback is added to the waitlist of AuthorizationSession when
  // `ipp_endpoint` must wait for the access token from it.
  void OnAccessTokenForEndpointCallback(const chromeos::Uri& ipp_endpoint,
                                        StatusCode status,
                                        std::string data);

  // Tries to find OAuth session for given IPP Endpoint and send Token Exchange
  // request to obtain an endpoint access token.
  void AttemptTokenExchange(IppEndpointTokenFetcher* it_endpoint);

  // Finds an element in `pending_authorizations_` with given `state` and remove
  // it. Returns false if such element does not exists. Otherwise, returns true
  // and returns the content of the element in the last two parameters.
  bool FindAndRemovePendingAuthorization(const std::string& state,
                                         base::flat_set<std::string>& scopes,
                                         std::string& code_verifier);

  // Represents started authorization procedure waiting for opening
  // communication with the server. This object is created when
  // InitAuthorization() is called and its callback does not return yet.
  struct WaitingAuthorization {
    WaitingAuthorization(base::flat_set<std::string>&& scopes,
                         StatusCallback callback);
    WaitingAuthorization(const WaitingAuthorization&) = delete;
    WaitingAuthorization& operator=(const WaitingAuthorization&) = delete;
    ~WaitingAuthorization();
    base::flat_set<std::string> scopes;
    StatusCallback callback;
  };

  // Represents started authorization procedure. This object is created when
  // InitAuthorization() is called and is destroyed in the corresponding
  // FinishAuthorization() call.
  struct PendingAuthorization {
    PendingAuthorization(base::flat_set<std::string>&& scopes,
                         std::string&& state,
                         std::string&& code_verifier);
    PendingAuthorization(const PendingAuthorization&) = delete;
    PendingAuthorization& operator=(const PendingAuthorization&) = delete;
    ~PendingAuthorization();
    base::flat_set<std::string> scopes;
    // Values used in the current authorization process.
    std::string state;
    std::string code_verifier;
  };

  // Holds basic parameters of the Authorization Server.
  AuthorizationServerData server_data_;

  // List of InitAuthorization() calls being processed.
  std::list<WaitingAuthorization> waiting_authorizations_;

  // List of completed InitAuthorization() calls waiting for the corresponding
  // FinishAuthorization() call.
  std::list<PendingAuthorization> pending_authorizations_;

  // List of active OAuth2 sessions.
  std::list<std::unique_ptr<AuthorizationServerSession>> sessions_;

  // List of IPP Endpoints for which an endpoint access token was requested.
  std::map<chromeos::Uri, std::unique_ptr<IppEndpointTokenFetcher>>
      ipp_endpoints_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_
