// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zone_impl.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/ash/printing/oauth2/authorization_server_session.h"
#include "chrome/browser/ash/printing/oauth2/client_ids_database.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
#include "chrome/browser/ash/printing/oauth2/ipp_endpoint_token_fetcher.h"
#include "chromeos/printing/uri.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace ash {
namespace printing {
namespace oauth2 {

namespace {

constexpr size_t kLengthOfCodeVerifier = 64;
constexpr size_t kLengthOfState = 16;

// Returns random string in Base64 format.
// `length` must be divisible by 4.
template <size_t length>
std::string RandBase64String() {
  static_assert(length % 4 == 0);
  static_assert(length > 3);
  return base::Base64Encode(crypto::RandBytesAsVector(length * 3 / 4));
}

// The code challenge created with the algorithm S256 (see RFC7636-4.2).
// RFC7636-4.1 specifies the requirements for `code_verifier`:
// "code_verifier = high-entropy cryptographic random STRING using the
// unreserved characters [A-Z] / [a-z] / [0-9] / "-" / "." / "_" / "~"
// from Section 2.3 of [RFC3986], with a minimum length of 43 characters
// and a maximum length of 128 characters."
std::string CodeChallengeS256(const std::string& code_verifier) {
  DCHECK_GE(code_verifier.size(), 43u);
  DCHECK_LE(code_verifier.size(), 128u);
  return base::Base64Encode(crypto::SHA256HashString(code_verifier));
}

// Builds and returns URL for Authorization Request (see RFC6749-4.1) with
// a code challenge (see RFC7636-4).
std::string GetAuthorizationURL(const AuthorizationServerData& server_data,
                                const base::flat_set<std::string>& scope,
                                const std::string& state,
                                const std::string& code_verifier) {
  chromeos::Uri uri(server_data.AuthorizationEndpointURI().spec());
  auto query = uri.GetQuery();
  query.push_back(std::make_pair("response_type", "code"));
  query.push_back(std::make_pair("response_mode", "query"));
  query.push_back(std::make_pair("client_id", server_data.ClientId()));
  query.push_back(std::make_pair("redirect_uri", kRedirectURI));
  if (!scope.empty()) {
    std::vector<std::string> parts(scope.begin(), scope.end());
    query.push_back(std::make_pair("scope", base::JoinString(parts, " ")));
  }
  query.push_back(std::make_pair("state", state));
  query.push_back(
      std::make_pair("code_challenge", CodeChallengeS256(code_verifier)));
  query.push_back(std::make_pair("code_challenge_method", "S256"));
  uri.SetQuery(query);
  return uri.GetNormalized(false);
}

// Tries to extract the parameter `name` from `query`. Returns the value of
// extracted parameter or an error message. `query` cannot contain empty
// vectors, but the vectors may contain empty strings.
base::expected<std::string, std::string> ExtractParameter(
    const base::flat_map<std::string, std::vector<std::string>>& query,
    const std::string& name) {
  auto it = query.find(name);
  if (it == query.end()) {
    return base::unexpected(
        base::StrCat({"parameter '", name, "' is missing"}));
  }
  if (it->second.size() != 1) {
    return base::unexpected(
        base::StrCat({"parameter '", name, "' is duplicated"}));
  }
  std::string value = it->second.front();
  if (value.empty()) {
    return base::unexpected(base::StrCat({"parameter '", name, "' is empty"}));
  }
  return base::ok(std::move(value));
}

// Calls `callback` with `status` and `data` as parameters. When `status` equals
// StatusCode::kOK, ignores `data` and passes an empty string instead.
void NoDataForOK(StatusCallback callback, StatusCode status, std::string data) {
  std::move(callback).Run(status,
                          (status == StatusCode::kOK) ? "" : std::move(data));
}

}  // namespace

AuthorizationZoneImpl::WaitingAuthorization::WaitingAuthorization(
    base::flat_set<std::string>&& scopes,
    StatusCallback callback)
    : scopes(scopes), callback(std::move(callback)) {}
AuthorizationZoneImpl::WaitingAuthorization::~WaitingAuthorization() = default;

AuthorizationZoneImpl::PendingAuthorization::PendingAuthorization(
    base::flat_set<std::string>&& scopes,
    std::string&& state,
    std::string&& code_verifier)
    : scopes(scopes), state(state), code_verifier(code_verifier) {}
AuthorizationZoneImpl::PendingAuthorization::~PendingAuthorization() = default;

AuthorizationZoneImpl::AuthorizationZoneImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& authorization_server_uri,
    ClientIdsDatabase* client_ids_database)
    : server_data_(url_loader_factory,
                   authorization_server_uri,
                   client_ids_database),
      url_loader_factory_(url_loader_factory) {}

AuthorizationZoneImpl::~AuthorizationZoneImpl() = default;

void AuthorizationZoneImpl::InitAuthorization(const std::string& scope,
                                              StatusCallback callback) {
  DCHECK_LE(waiting_authorizations_.size(), kMaxNumberOfSessions);

  // If there are too many callbacks waiting, remove the oldest one.
  if (waiting_authorizations_.size() == kMaxNumberOfSessions) {
    std::move(waiting_authorizations_.front().callback)
        .Run(StatusCode::kTooManySessions,
             "Authorization Zone not initialized");
    waiting_authorizations_.pop_front();
  }

  // Add the callback to the waiting list.
  waiting_authorizations_.emplace_back(ParseScope(scope), std::move(callback));
  if (waiting_authorizations_.size() == 1) {
    // The list was empty and it is the first element.
    // Check if the server is ready.
    if (server_data_.IsReady()) {
      // The server is ready. Just go ahead and process the element.
      AuthorizationProcedure();
    } else {
      // The server must be initialized (it is the very first call to
      // InitAuthorization()). AuthorizationProcedure() will be called inside
      // OnInitializeCallback().
      // We can use base::Unretained() here because:
      // * AuthorizationServerData (and HttpExchange) guarantees that no calls
      //   will be returned after deletion of the object `server_data_`.
      // * `this` owns `server_data_`; it is guaranteed that deletion of
      //   `server_data_` is performed before deletion of `this`.
      server_data_.Initialize(
          base::BindOnce(&AuthorizationZoneImpl::OnInitializeCallback,
                         base::Unretained(this)));
    }
  }
}

void AuthorizationZoneImpl::FinishAuthorization(const GURL& redirect_url,
                                                StatusCallback callback) {
  // Parse the URL and retrieve the query segment.
  chromeos::Uri uri(redirect_url.spec());
  if (uri.GetLastParsingError().status !=
      chromeos::Uri::ParserStatus::kNoErrors) {
    std::move(callback).Run(StatusCode::kInvalidResponse,
                            "Authorization Request: cannot parse obtained URL");
    return;
  }
  const auto query = uri.GetQueryAsMap();

  // Extract the parameter "state".
  ASSIGN_OR_RETURN(const std::string state, ExtractParameter(query, "state"),
                   [&](std::string error) {
                     std::move(callback).Run(
                         StatusCode::kInvalidResponse,
                         "Authorization Request: " + std::move(error));
                   });

  // Use `state` to match pending authorization.
  base::flat_set<std::string> scopes;
  std::string code_verifier;
  if (!FindAndRemovePendingAuthorization(state, scopes, code_verifier)) {
    std::move(callback).Run(StatusCode::kNoMatchingSession,
                            "Authorization Request");
    return;
  }

  // Check if the parameter "error" is present. If yes, try to extract the error
  // message.
  if (query.contains("error")) {
    ASSIGN_OR_RETURN(const std::string error, ExtractParameter(query, "error"),
                     [&](std::string error) {
                       std::move(callback).Run(
                           StatusCode::kInvalidResponse,
                           "Authorization Request: " + std::move(error));
                     });

    StatusCode status;
    if (error == "server_error") {
      status = StatusCode::kServerError;
    } else if (error == "temporarily_unavailable") {
      status = StatusCode::kServerTemporarilyUnavailable;
    } else {
      status = StatusCode::kAccessDenied;
    }
    std::move(callback).Run(status, "Authorization Request: error=" + error);
    return;
  }

  // Extract the parameter "code".
  ASSIGN_OR_RETURN(const std::string code, ExtractParameter(query, "code"),
                   [&](std::string error) {
                     std::move(callback).Run(
                         StatusCode::kInvalidResponse,
                         "Authorization Request: " + std::move(error));
                   });

  // Create and add a new session.
  if (sessions_.size() == kMaxNumberOfSessions) {
    // There are too many sessions. Remove the oldest one.
    auto sessions_callbacks = sessions_.front()->TakeWaitingList();
    sessions_.pop_front();
    for (auto& sessions_callback : sessions_callbacks) {
      std::move(sessions_callback)
          .Run(StatusCode::kTooManySessions, "The oldest session was closed");
    }
    // TODO(b:228876367) - revoke the token in AuthorizationServerSession
  }
  sessions_.push_back(std::make_unique<AuthorizationServerSession>(
      url_loader_factory_, server_data_.TokenEndpointURI(), std::move(scopes)));
  AuthorizationServerSession* session = sessions_.back().get();
  session->AddToWaitingList(base::BindOnce(&NoDataForOK, std::move(callback)));
  // We can use base::Unretained() here because:
  // * AuthorizationServerSession (and HttpExchange) guarantees that no calls
  //   will be returned after deletion of the object `session`.
  // * `this` owns `session`; it is guaranteed that deletion of `session` is
  //   performed before deletion of `this`.
  session->SendFirstTokenRequest(
      server_data_.ClientId(), code, code_verifier,
      base::BindOnce(&AuthorizationZoneImpl::OnSendTokenRequestCallback,
                     base::Unretained(this), base::Unretained(session)));
}

void AuthorizationZoneImpl::GetEndpointAccessToken(
    const chromeos::Uri& ipp_endpoint,
    const std::string& scope,
    StatusCallback callback) {
  // Try to find the IPP Endpoint.
  auto it = ipp_endpoints_.find(ipp_endpoint);

  if (it == ipp_endpoints_.end()) {
    // IPP Endpoint is not known. Create a new IppEndpointFetcher.
    auto ptr = std::make_unique<IppEndpointTokenFetcher>(
        url_loader_factory_, server_data_.TokenEndpointURI(), ipp_endpoint,
        ParseScope(scope));
    IppEndpointTokenFetcher* endpoint = ptr.get();
    it = ipp_endpoints_.emplace(ipp_endpoint, std::move(ptr)).first;
    endpoint->AddToWaitingList(std::move(callback));
    AttemptTokenExchange(endpoint);
    return;
  }

  IppEndpointTokenFetcher* endpoint = it->second.get();
  if (endpoint->endpoint_access_token().empty()) {
    // Endpoint Access Token is not ready yet.
    endpoint->AddToWaitingList(std::move(callback));
    return;
  }

  std::move(callback).Run(StatusCode::kOK, endpoint->endpoint_access_token());
}

void AuthorizationZoneImpl::MarkEndpointAccessTokenAsExpired(
    const chromeos::Uri& ipp_endpoint,
    const std::string& endpoint_access_token) {
  if (endpoint_access_token.empty()) {
    return;
  }
  auto it = ipp_endpoints_.find(ipp_endpoint);
  if (it == ipp_endpoints_.end()) {
    return;
  }
  IppEndpointTokenFetcher* endpoint = it->second.get();
  if (endpoint->endpoint_access_token() == endpoint_access_token) {
    ipp_endpoints_.erase(it);
  }
}

void AuthorizationZoneImpl::AuthorizationProcedure() {
  DCHECK_LE(pending_authorizations_.size(), kMaxNumberOfSessions);

  for (auto& wa : waiting_authorizations_) {
    // Remove the oldest pending authorization if there are too many of them.
    if (pending_authorizations_.size() == kMaxNumberOfSessions) {
      pending_authorizations_.pop_front();
    }
    // Create new pending authorization and call the callback with an
    // authorization URL to open in the browser.
    PendingAuthorization& pa = pending_authorizations_.emplace_back(
        std::move(wa.scopes), RandBase64String<kLengthOfState>(),
        RandBase64String<kLengthOfCodeVerifier>());
    std::string auth_url = GetAuthorizationURL(server_data_, pa.scopes,
                                               pa.state, pa.code_verifier);
    std::move(wa.callback).Run(StatusCode::kOK, std::move(auth_url));
  }
  waiting_authorizations_.clear();
}

void AuthorizationZoneImpl::MarkAuthorizationZoneAsUntrusted() {
  const std::string msg = "Authorization Server marked as untrusted";

  pending_authorizations_.clear();

  // This method will call all callbacks from `waiting_authorizations_` and
  // empty it.
  OnInitializeCallback(StatusCode::kUntrustedAuthorizationServer, msg);

  // Clear `sessions_`.
  for (std::unique_ptr<AuthorizationServerSession>& session : sessions_) {
    std::vector<StatusCallback> callbacks = session->TakeWaitingList();
    for (StatusCallback& callback : callbacks) {
      std::move(callback).Run(StatusCode::kUntrustedAuthorizationServer, msg);
    }
  }
  sessions_.clear();

  // Clear `ipp_endpoints_`.
  for (auto& [_, ipp_endpoint] : ipp_endpoints_) {
    std::vector<StatusCallback> callbacks = ipp_endpoint->TakeWaitingList();
    for (StatusCallback& callback : callbacks) {
      std::move(callback).Run(StatusCode::kUntrustedAuthorizationServer, msg);
    }
  }
  ipp_endpoints_.clear();
}

void AuthorizationZoneImpl::OnInitializeCallback(StatusCode status,
                                                 std::string data) {
  if (status == StatusCode::kOK) {
    AuthorizationProcedure();
    return;
  }
  // Something went wrong. Return error.
  for (auto& wa : waiting_authorizations_) {
    std::move(wa.callback).Run(status, data);
  }
  waiting_authorizations_.clear();
}

void AuthorizationZoneImpl::OnSendTokenRequestCallback(
    AuthorizationServerSession* session,
    StatusCode status,
    std::string data) {
  // Find the session for which the request was completed.
  auto it_session = base::ranges::find(
      sessions_, session, &std::unique_ptr<AuthorizationServerSession>::get);
  DCHECK(it_session != sessions_.end());

  // Get the list of callbacks to run and copy the data.
  std::vector<StatusCallback> callbacks = session->TakeWaitingList();
  // Erase the session if the request failed.
  if (status != StatusCode::kOK) {
    sessions_.erase(it_session);
  }
  // Run the callbacks.
  for (auto& callback : callbacks) {
    std::move(callback).Run(status, data);
  }
}

void AuthorizationZoneImpl::OnTokenExchangeRequestCallback(
    const chromeos::Uri& ipp_endpoint,
    StatusCode status,
    std::string data) {
  if (status == StatusCode::kInvalidAccessToken) {
    // The access token used by IppEndpointFetcher is invalid. Find the session
    // the token came from and ask it to refresh the token.
    for (std::unique_ptr<AuthorizationServerSession>& session : sessions_) {
      if (session->access_token() == data) {  // data == invalid access token
        // We can use base::Unretained() here because:
        // * AuthorizationServerSession (and HttpExchange) guarantees that no
        //   calls will be returned after deletion of the object `session`.
        // * `this` owns `session`; it is guaranteed that deletion of `session`
        //   is performed before deletion of `this`.
        session->SendNextTokenRequest(base::BindOnce(
            &AuthorizationZoneImpl::OnSendTokenRequestCallback,
            base::Unretained(this), base::Unretained(session.get())));
      }
    }

    // Find the corresponding IppEndpointTokenFetcher object.
    auto it_endpoint = ipp_endpoints_.find(ipp_endpoint);
    DCHECK(it_endpoint != ipp_endpoints_.end());
    IppEndpointTokenFetcher* endpoint = it_endpoint->second.get();

    // Try to find a new session for IPP endpoint and perform Token Exchange
    // again.
    AttemptTokenExchange(endpoint);
    return;
  }
  // For all other statuses just send back the result.
  ResultForIppEndpoint(ipp_endpoint, status, std::move(data));
}

void AuthorizationZoneImpl::ResultForIppEndpoint(
    const chromeos::Uri& ipp_endpoint,
    StatusCode status,
    std::string data) {
  auto it = ipp_endpoints_.find(ipp_endpoint);
  DCHECK(it != ipp_endpoints_.end());
  // The list of callbacks to run.
  std::vector<StatusCallback> callbacks = it->second->TakeWaitingList();
  // Erase the IPP Endpoint in case of an error.
  if (status != StatusCode::kOK) {
    ipp_endpoints_.erase(it);
  }
  // Run the callbacks.
  for (auto& callback : callbacks) {
    std::move(callback).Run(status, data);
  }
}

void AuthorizationZoneImpl::OnAccessTokenForEndpointCallback(
    const chromeos::Uri& ipp_endpoint,
    StatusCode status,
    std::string data) {
  auto it = ipp_endpoints_.find(ipp_endpoint);
  DCHECK(it != ipp_endpoints_.end());
  IppEndpointTokenFetcher* endpoint = it->second.get();

  switch (status) {
    case StatusCode::kOK:
      // We got a new access token (in `data`). Now, we can get a new endpoint
      // access token.
      // We can use base::Unretained() here because:
      // * IppEndpointTokenFetcher (and HttpExchange) guarantees that no
      //   calls will be returned after deletion of the object `endpoint`.
      // * `this` owns `endpoint`; it is guaranteed that deletion of `endpoint`
      //   is performed before deletion of `this`.
      endpoint->SendTokenExchangeRequest(
          data,
          base::BindOnce(&AuthorizationZoneImpl::OnTokenExchangeRequestCallback,
                         base::Unretained(this), ipp_endpoint));
      break;
    case StatusCode::kInvalidAccessToken:
    case StatusCode::kTooManySessions:
      // The session timed out. Try to find other session to get an access
      // token.
      AttemptTokenExchange(endpoint);
      break;
    default:
      // For all other statuses just send back the result.
      ResultForIppEndpoint(ipp_endpoint, status, std::move(data));
      break;
  }
}

void AuthorizationZoneImpl::AttemptTokenExchange(
    IppEndpointTokenFetcher* endpoint) {
  AuthorizationServerSession* auth_session = nullptr;
  // Try to match a session starting from the newest one.
  for (auto& session : base::Reversed(sessions_)) {
    if (session->ContainsAll(endpoint->scope())) {
      auth_session = session.get();
      break;
    }
  }
  if (!auth_session) {
    // No matching sessions. Inform the callers that a new session must be
    // created.
    ResultForIppEndpoint(endpoint->ipp_endpoint_uri(),
                         StatusCode::kAuthorizationNeeded, "");
    return;
  }

  // We found a session to use. Get its access token.
  const auto access_token = auth_session->access_token();
  if (access_token.empty()) {
    // Access token not ready. Add the IPP Endpoint to the waiting list.
    // We can use base::Unretained() here because:
    // * AuthorizationServerSession (and HttpExchange) guarantees that no
    //   calls will be returned after deletion of the object `auth_session`.
    // * `this` owns `auth_session`; it is guaranteed that deletion of
    //   `auth_session` is performed before deletion of `this`.
    auth_session->AddToWaitingList(
        base::BindOnce(&AuthorizationZoneImpl::OnAccessTokenForEndpointCallback,
                       base::Unretained(this), endpoint->ipp_endpoint_uri()));
    return;
  }

  // Try to use the access token to get a new endpoint access token.
  // We can use base::Unretained() here because:
  // * IppEndpointTokenFetcher (and HttpExchange) guarantees that no
  //   calls will be returned after deletion of the object `endpoint`.
  // * `this` owns `endpoint`; it is guaranteed that deletion of `endpoint`
  //   is performed before deletion of `this`.
  endpoint->SendTokenExchangeRequest(
      access_token,
      base::BindOnce(&AuthorizationZoneImpl::OnTokenExchangeRequestCallback,
                     base::Unretained(this), endpoint->ipp_endpoint_uri()));
}

bool AuthorizationZoneImpl::FindAndRemovePendingAuthorization(
    const std::string& state,
    base::flat_set<std::string>& scopes,
    std::string& code_verifier) {
  std::list<PendingAuthorization>::iterator it = base::ranges::find(
      pending_authorizations_, state, &PendingAuthorization::state);
  if (it == pending_authorizations_.end()) {
    return false;
  }
  scopes = std::move(it->scopes);
  code_verifier = std::move(it->code_verifier);
  pending_authorizations_.erase(it);
  return true;
}

std::unique_ptr<AuthorizationZone> AuthorizationZone::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& authorization_server_uri,
    ClientIdsDatabase* client_ids_database) {
  return std::make_unique<AuthorizationZoneImpl>(
      url_loader_factory, authorization_server_uri, client_ids_database);
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
