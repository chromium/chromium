// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

class ClientIdsDatabase;

// This class is responsible for the initial communication with the
// Authorization Server specified in the constructor. The method Initialize(...)
// retrieves metadata from the server and tries to register to it as a new
// client if necessary.
class AuthorizationServerData {
 public:
  // `client_ids_database` cannot be nullptr and must outlive this object.
  AuthorizationServerData(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& authorization_server_uri,
      ClientIdsDatabase* client_ids_database);

  AuthorizationServerData(const AuthorizationServerData&) = delete;
  AuthorizationServerData& operator=(const AuthorizationServerData&) = delete;
  ~AuthorizationServerData();

  // Accessors to the server's metadata.
  const GURL& AuthorizationServerURI() const {
    return authorization_server_uri_;
  }
  std::string ClientId() const { return client_id_.value_or(""); }
  const GURL& AuthorizationEndpointURI() const {
    return authorization_endpoint_uri_;
  }
  const GURL& TokenEndpointURI() const { return token_endpoint_uri_; }
  const GURL& RegistrationEndpointURI() const {
    return registration_endpoint_uri_;
  }
  const GURL& RevocationEndpointURI() const { return revocation_endpoint_uri_; }

  // Returns true <=> the connection with the server was successfully
  // initialized. It is true <=> the method Initialize(...) was called earlier
  // and its callback returned StatusCode::kOK.
  bool IsReady() const {
    return !(!client_id_ || authorization_endpoint_uri_.is_empty() ||
             token_endpoint_uri_.is_empty() || client_id_->empty());
  }

  // Downloads metadata from the server. It also tries to register a new client
  // to the server if the server is not present in `client_ids_database_` (see
  // the constructor). If the client is not registered (i.e. the server is not
  // present in the database) and the server does not support dynamic
  // registration the callback returns StatusCode::kClientNotRegistered. If the
  // registration succeeds the obtained `client_id_` along with
  // `authorization_endpoint_uri_` is saved in `client_ids_database_`.
  // `callback` must be non-empty. This method cannot be called again before the
  // `callback` returns.
  void Initialize(StatusCallback callback);

 private:
  // Loads metadata from the server if `authorization_endpoint_uri_` or
  // `token_endpoint_uri_` are empty. Also tries to register the client to the
  // server if `client_id_` is empty. Calls `callback_` with results.
  void InitializationProcedure();
  // Sets `client_id_` to the value fetched from `client_ids_database_`.
  void OnClientIdFetched(StatusCode status, std::string data);
  // Prepares and sends Metadata Request.
  void SendMetadataRequest();
  // Analyzes response for Metadata Request.
  void OnMetadataResponse(StatusCode status);
  // Prepares and sends Registration Request.
  void SendRegistrationRequest();
  // Analyzes response for Registration Request.
  void OnRegistrationResponse(StatusCode status);

  // Basic information about the Authorization Server.
  const GURL authorization_server_uri_;
  std::optional<std::string> client_id_;

  // Metadata read from the Authorization Server.
  GURL authorization_endpoint_uri_;
  GURL token_endpoint_uri_;
  GURL registration_endpoint_uri_;
  GURL revocation_endpoint_uri_;

  StatusCallback callback_;

  // The object used to fetch and store client_id.
  raw_ptr<ClientIdsDatabase> client_ids_database_;

  // The object used for communication with the Authorization Server.
  HttpExchange http_exchange_;

  base::WeakPtrFactory<AuthorizationServerData> weak_ptr_factory_{this};
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_
