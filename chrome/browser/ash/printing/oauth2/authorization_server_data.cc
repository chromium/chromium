// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace ash {
namespace printing {
namespace oauth2 {

AuthorizationServerData::AuthorizationServerData(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& authorization_server_uri,
    ClientIdsDatabase* client_ids_database)
    : authorization_server_uri_(authorization_server_uri),
      client_ids_database_(client_ids_database),
      http_exchange_(url_loader_factory) {
  CHECK(client_ids_database_);
}

AuthorizationServerData::~AuthorizationServerData() = default;

void AuthorizationServerData::Initialize(StatusCallback callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = std::move(callback);
  InitializationProcedure();
}

void AuthorizationServerData::InitializationProcedure() {
  // First, check if `client_id_` is known.
  if (!client_id_) {
    client_ids_database_->FetchId(
        authorization_server_uri_,
        base::BindOnce(&AuthorizationServerData::OnClientIdFetched,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Check if we have server's metadata.
  if (authorization_endpoint_uri_.is_empty() ||
      token_endpoint_uri_.is_empty()) {
    SendMetadataRequest();
    return;
  }

  // Check if the `client_id_` is known. If not, try to register a new client
  // and obtain `client_id_`. Return error when the server doesn't support
  // dynamic registration.
  if (client_id_->empty()) {
    if (registration_endpoint_uri_.is_empty()) {
      std::move(callback_).Run(StatusCode::kClientNotRegistered, "");
    } else {
      SendRegistrationRequest();
    }
    return;
  }

  // Everything is done already, just call the callback.
  std::move(callback_).Run(StatusCode::kOK, "");
}

void AuthorizationServerData::OnClientIdFetched(StatusCode status,
                                                std::string data) {
  if (status != StatusCode::kOK) {
    // Error occurred. Exit.
    std::move(callback_).Run(status, data);
    return;
  }

  // Success! Set `client_id_` and return to the main procedure.
  client_id_ = data;
  InitializationProcedure();
}

void AuthorizationServerData::SendMetadataRequest() {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "printing_oauth2_metadata_request", "printing_oauth2_http_exchange",
          R"(
    semantics {
      description:
        "This request downloads settings of Authorization Server."
      data:
        "No data are sent."
    })");
  http_exchange_.Clear();
  // Add .well-known prefix to the path, see RFC 8414 (section 3) and RFC 8615.
  chromeos::Uri uri(authorization_server_uri_.spec());
  const std::vector<std::string> prefix = {".well-known",
                                           "oauth-authorization-server"};
  auto path = uri.GetPath();
  path.insert(path.begin(), prefix.begin(), prefix.end());
  uri.SetPath(path);
  http_exchange_.Exchange(
      "GET", GURL(uri.GetNormalized()), ContentFormat::kEmpty, 200, -1,
      partial_traffic_annotation,
      base::BindOnce(&AuthorizationServerData::OnMetadataResponse,
                     base::Unretained(this)));
}

void AuthorizationServerData::OnMetadataResponse(StatusCode status) {
  if (status != StatusCode::kOK) {
    // Error occurred. Exit.
    std::move(callback_).Run(
        status, "Metadata Request: " + http_exchange_.GetErrorMessage());
    return;
  }

  // Parse the response.
  const bool ok =
      http_exchange_.ParamURLEquals("issuer", true,
                                    authorization_server_uri_) &&
      http_exchange_.ParamURLGet("authorization_endpoint", true,
                                 &authorization_endpoint_uri_) &&
      http_exchange_.ParamURLGet("token_endpoint", true,
                                 &token_endpoint_uri_) &&
      http_exchange_.ParamURLGet("registration_endpoint", false,
                                 &registration_endpoint_uri_) &&
      http_exchange_.ParamArrayStringContains("response_types_supported", true,
                                              "code") &&
      http_exchange_.ParamArrayStringContains("response_modes_supported", false,
                                              "query") &&
      http_exchange_.ParamArrayStringContains("grant_types_supported", false,
                                              "authorization_code") &&
      http_exchange_.ParamArrayStringContains(
          "token_endpoint_auth_methods_supported", true, "none") &&
      http_exchange_.ParamURLGet("revocation_endpoint", false,
                                 &revocation_endpoint_uri_) &&
      http_exchange_.ParamArrayStringContains(
          "revocation_endpoint_auth_methods_supported", false, "none") &&
      http_exchange_.ParamArrayStringContains(
          "code_challenge_methods_supported", true, "S256");
  if (!ok) {
    // Parsing failed. Reset all parameters and exit.
    authorization_endpoint_uri_ = token_endpoint_uri_ = GURL();
    registration_endpoint_uri_ = revocation_endpoint_uri_ = GURL();
    std::move(callback_).Run(
        StatusCode::kInvalidResponse,
        "Metadata Request: " + http_exchange_.GetErrorMessage());
    return;
  }

  // Success! Return to the main procedure.
  InitializationProcedure();
}

void AuthorizationServerData::SendRegistrationRequest() {
  net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation =
      net::DefinePartialNetworkTrafficAnnotation(
          "printing_oauth2_registration_request",
          "printing_oauth2_http_exchange", R"(
    semantics {
      description:
        "This request registers this client to the new Authorization Server."
      data:
        "No local data are sent."
    })");
  http_exchange_.Clear();
  http_exchange_.AddParamArrayString("redirect_uris", {kRedirectURI});
  http_exchange_.AddParamArrayString("token_endpoint_auth_method", {"none"});
  http_exchange_.AddParamArrayString("grant_types", {"authorization_code"});
  http_exchange_.AddParamArrayString("response_types", {"code"});
  http_exchange_.AddParamString("client_name", kClientName);
  http_exchange_.Exchange(
      "POST", registration_endpoint_uri_, ContentFormat::kJson, 201, 400,
      partial_traffic_annotation,
      base::BindOnce(&AuthorizationServerData::OnRegistrationResponse,
                     base::Unretained(this)));
}

void AuthorizationServerData::OnRegistrationResponse(StatusCode status) {
  if (status != StatusCode::kOK) {
    // Error occurred. Exit.
    std::move(callback_).Run(
        status, "Registration Request: " + http_exchange_.GetErrorMessage());
    return;
  }

  // Parse the response.
  const bool ok =
      http_exchange_.ParamStringGet("client_id", true, &*client_id_) &&
      http_exchange_.ParamArrayStringEquals("redirect_uris", true,
                                            {kRedirectURI}) &&
      http_exchange_.ParamArrayStringEquals("token_endpoint_auth_method", true,
                                            {"none"}) &&
      http_exchange_.ParamArrayStringEquals("grant_types", true,
                                            {"authorization_code"}) &&
      http_exchange_.ParamArrayStringEquals("response_types", true, {"code"}) &&
      http_exchange_.ParamStringEquals("client_name", true, kClientName);
  if (!ok) {
    // Parsing failed. Reset all parameters and exit.
    client_id_->clear();
    std::move(callback_).Run(
        StatusCode::kInvalidResponse,
        "Registration Request: " + http_exchange_.GetErrorMessage());
    return;
  }

  // Save the obtained `client_id_`.
  client_ids_database_->StoreId(authorization_server_uri_, *client_id_);

  // Success! Return to the main procedure.
  InitializationProcedure();
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
