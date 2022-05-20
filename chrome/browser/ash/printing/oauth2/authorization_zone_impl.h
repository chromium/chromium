// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_

#include <list>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"
#include "chrome/browser/ash/printing/oauth2/authorization_server_session.h"
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

// The class AuthorizationZoneImpl implements functionality described in
// AuthorizationZone interface.
//
class AuthorizationZoneImpl : public AuthorizationZone {
 public:
  // Constructor. If `client_id` is empty a Registration Request will be used
  // to register a new client (inside InitAuthorization(...) method).
  AuthorizationZoneImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& authorization_server_uri,
      const std::string& client_id);
  // Not copyable.
  AuthorizationZoneImpl(const AuthorizationZoneImpl&) = delete;
  AuthorizationZoneImpl& operator=(const AuthorizationZoneImpl&) = delete;
  // Destructor.
  ~AuthorizationZoneImpl() override;

  // AuthorizationZone interface.
  void InitAuthorization(const std::string& scope,
                         StatusCallback callback) override;
  // AuthorizationZone interface.
  void FinishAuthorization(const GURL& redirect_url,
                           StatusCallback callback) override;
  // AuthorizationZone interface.
  void GetEndpointAccessToken(const chromeos::Uri& ipp_endpoint,
                              const std::string& scope,
                              StatusCallback callback) override;
  // AuthorizationZone interface.
  void MarkEndpointAccessTokenAsExpired(
      const chromeos::Uri& ipp_endpoint,
      const std::string& endpoint_access_token) override;

 private:
  // This method processes (and removes) all elements from
  // waiting_authorizations_ by initiating authorization procedures for them.
  void AuthorizationProcedure();

  // Callback for AuthorizationServerData::Initialize(...).
  void OnInitializeCallback(StatusCode status, const std::string& data);

  // Adds context info to error messages returned with `callback`.
  void AddContextToErrorMessage(StatusCallback& callback);

  // Represents started authorization procedure waiting for opening
  // communication with the server. This object is created when
  // InitAuthorization(...) is called and its callback does not return yet.
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
  // InitAuthorization(...) is called and is destroyed in the corresponding
  // FinishAuthorization(...) call.
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

  // List of InitAuthorization(...) calls being processed.
  std::list<WaitingAuthorization> waiting_authorizations_;

  // List of completed InitAuthorization(...) calls waiting for
  // the corresponding FinishAuthorization(...) call.
  std::list<PendingAuthorization> pending_authorizations_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_ZONE_IMPL_H_
