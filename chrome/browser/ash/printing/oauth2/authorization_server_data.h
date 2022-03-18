// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_

#include <memory>
#include <string>

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

// This class is responsible for the initial communication with the
// Authorization Server specified in the constructor. The method Initialize(...)
// retrieves metadata from the server and tries to register to it as a new
// client if necessary.
class AuthorizationServerData {
 public:
  // Constructor. Empty `client_id` means that this client is not known to the
  // server and must be registered.
  AuthorizationServerData(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& authorization_server_uri,
      const std::string& client_id);

  // Copying and moving is not allowed.
  AuthorizationServerData(const AuthorizationServerData&) = delete;
  AuthorizationServerData& operator=(const AuthorizationServerData&) = delete;

  // Destructor.
  ~AuthorizationServerData();

  // Accessors to the server's metadata.
  const GURL& AuthorizationServerURI() const {
    return authorization_server_uri_;
  }
  const std::string ClientId() const { return client_id_; }
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
    return !(authorization_endpoint_uri_.is_empty() ||
             token_endpoint_uri_.is_empty() || client_id_.empty());
  }

  // Downloads metadata from the server. It also tries to register a new client
  // to the server if the parameter `client_id` in the constructor was empty.
  // If the parameter `client_id` in the constructor was empty and the server
  // does not support dynamic registration the callback returns
  // StatusCode::kClientNotRegistered.
  void Initialize(StatusCallback callback);

 private:
  // Loads metadata from the server if `authorization_endpoint_uri_` or
  // `token_endpoint_uri_` are empty. Also tries to register the client to the
  // server if `client_id_` is empty. Calls `callback_` with results.
  void InitializationProcedure();
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
  std::string client_id_;

  // Metadata read from the Authorization Server.
  GURL authorization_endpoint_uri_;
  GURL token_endpoint_uri_;
  GURL registration_endpoint_uri_;
  GURL revocation_endpoint_uri_;

  StatusCallback callback_;

  // The object used for communication with the Authorization Server.
  HttpExchange http_exchange_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_AUTHORIZATION_SERVER_DATA_H_
