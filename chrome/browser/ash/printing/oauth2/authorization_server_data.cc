// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_server_data.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
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
    const std::string& client_id)
    : authorization_server_uri_(authorization_server_uri),
      client_id_(client_id),
      http_exchange_(url_loader_factory) {}

AuthorizationServerData::~AuthorizationServerData() = default;

void AuthorizationServerData::Initialize(StatusCallback callback) {
  callback_ = std::move(callback);
  InitializationProcedure();
}

void AuthorizationServerData::InitializationProcedure() {
  // First, check if we have server's metadata.
  if (authorization_endpoint_uri_.is_empty() ||
      token_endpoint_uri_.is_empty()) {
    SendMetadataRequest();
    return;
  }

  // Check if the clientID is known. If not, tries to register a new client
  // and obtain clientID. Return error when the server doesn't support
  // dynamic registration.
  if (client_id_.empty()) {
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
      http_exchange_.ParamStringGet("client_id", true, &client_id_) &&
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
    client_id_.clear();
    std::move(callback_).Run(
        StatusCode::kInvalidResponse,
        "Registration Request: " + http_exchange_.GetErrorMessage());
    return;
  }

  // Success! Return to the main procedure.
  InitializationProcedure();
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
