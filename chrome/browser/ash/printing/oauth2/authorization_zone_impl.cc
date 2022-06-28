// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zone_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/printing/oauth2/authorization_server_session.h"
#include "chrome/browser/ash/printing/oauth2/constants.h"
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
  std::vector<uint8_t> bytes(length * 3 / 4);
  crypto::RandBytes(bytes);
  return base::Base64Encode(bytes);
}

// The code challenge created with the algorithm S256 (see RFC7636-4.2).
// RFC7636-4.1 specifies the requirements for `code_verifier`:
// "code_verifier = high-entropy cryptographic random STRING using the
// unreserved characters [A-Z] / [a-z] / [0-9] / "-" / "." / "_" / "~"
// from Section 2.3 of [RFC3986], with a minimum length of 43 characters
// and a maximum length of 128 characters."
std::string CodeChallengeS256(const std::string& code_verifier) {
  DCHECK_GE(code_verifier.size(), 43);
  DCHECK_LE(code_verifier.size(), 128);
  std::string output;
  base::Base64Encode(crypto::SHA256HashString(code_verifier), &output);
  return output;
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
  return value;
}

// Calls `callback` with `status` and `data` as parameters. When `status` equals
// StatusCode::kOK, ignores `data` and passes an empty string instead.
void NoDataForOK(StatusCallback callback,
                 StatusCode status,
                 const std::string& data) {
  std::move(callback).Run(status, (status == StatusCode::kOK) ? "" : data);
}

// Calls `callback`. Adds a prefix with `context` to an error sent in `data`.
void PrefixForError(StatusCallback callback,
                    const std::string& context,
                    StatusCode status,
                    const std::string& data) {
  std::string msg = status == StatusCode::kOK
                        ? data
                        : base::StrCat({"[", context, "] ", data});
  std::move(callback).Run(status, msg);
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
    const std::string& client_id)
    : server_data_(url_loader_factory, authorization_server_uri, client_id),
      url_loader_factory_(url_loader_factory) {}

AuthorizationZoneImpl::~AuthorizationZoneImpl() = default;

void AuthorizationZoneImpl::InitAuthorization(const std::string& scope,
                                              StatusCallback callback) {
  DCHECK_LE(waiting_authorizations_.size(), kMaxNumberOfSessions);
  AddContextToErrorMessage(callback);

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
      server_data_.Initialize(
          base::BindOnce(&AuthorizationZoneImpl::OnInitializeCallback,
                         base::Unretained(this)));
    }
  }
}

void AuthorizationZoneImpl::FinishAuthorization(const GURL& redirect_url,
                                                StatusCallback callback) {
  AddContextToErrorMessage(callback);

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
  base::expected<std::string, std::string> val_or_err =
      ExtractParameter(query, "state");
  if (!val_or_err.has_value()) {
    std::move(callback).Run(
        StatusCode::kInvalidResponse,
        base::StrCat({"Authorization Request: ", val_or_err.error()}));
    return;
  }
  const std::string state = std::move(val_or_err.value());

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
    val_or_err = ExtractParameter(query, "error");
    if (!val_or_err.has_value()) {
      std::move(callback).Run(
          StatusCode::kInvalidResponse,
          base::StrCat({"Authorization Request: ", val_or_err.error()}));
      return;
    }
    const std::string error = std::move(val_or_err.value());

    StatusCode status;
    if (error == "server_error") {
      status = StatusCode::kServerError;
    } else if (error == "temporarily_unavailable") {
      status = StatusCode::kServerTemporarilyUnavailable;
    } else {
      status = StatusCode::kAccessDenied;
    }
    std::move(callback).Run(
        status, base::StrCat({"Authorization Request: error=", error}));
    return;
  }

  // Extract the parameter "code".
  val_or_err = ExtractParameter(query, "code");
  if (!val_or_err.has_value()) {
    std::move(callback).Run(
        StatusCode::kInvalidResponse,
        base::StrCat({"Authorization Request: ", val_or_err.error()}));
    return;
  }
  const std::string code = std::move(val_or_err.value());

  // Create and add a new session.
  if (sessions_.size() == kMaxNumberOfSessions) {
    // There are too many sessions. Remove the oldest one.
    auto callbacks = sessions_.front()->TakeWaitingList();
    sessions_.pop_front();
    for (auto& callback : callbacks) {
      std::move(callback).Run(StatusCode::kTooManySessions,
                              "The oldest session was closed");
    }
    // TODO(b:228876367) - revoke the token in AuthorizationServerSession
  }
  sessions_.push_back(std::make_unique<AuthorizationServerSession>(
      url_loader_factory_, server_data_.TokenEndpointURI(), std::move(scopes)));
  AuthorizationServerSession* session = sessions_.back().get();
  session->AddToWaitingList(base::BindOnce(&NoDataForOK, std::move(callback)));
  // We can use base::Unretained() here because:
  // * Construction of AuthorizationServerSession (and HttpExchange) guarantees
  //   that no calls will be returned after deletion of the object `session`.
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
  AddContextToErrorMessage(callback);
  // TODO(pawliczek)
  // This method is supposed to return endpoint access token for given
  // `ipp_endpoint`. If the given `ipp_endpoint` is not known yet, this method
  // will request from the server a new endpoint access token for given
  // `ipp_endpoint` and `scope`.
}

void AuthorizationZoneImpl::MarkEndpointAccessTokenAsExpired(
    const chromeos::Uri& ipp_endpoint,
    const std::string& endpoint_access_token) {
  // TODO(pawliczek)
  // This method removes given `ipp_endpoint` from the list of known ipp
  // endpoints. It happens only if its endpoint access token equals
  // `endpoint_access_token`.
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
    auto& pa = pending_authorizations_.emplace_back(
        std::move(wa.scopes), RandBase64String<kLengthOfState>(),
        RandBase64String<kLengthOfCodeVerifier>());
    auto auth_url = GetAuthorizationURL(server_data_, pa.scopes, pa.state,
                                        pa.code_verifier);
    std::move(wa.callback).Run(StatusCode::kOK, auth_url);
  }
  waiting_authorizations_.clear();
}

void AuthorizationZoneImpl::OnInitializeCallback(StatusCode status,
                                                 const std::string& data) {
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
    const std::string& data) {
  // Find the session for which the request was completed.
  auto it_session = std::find_if(
      sessions_.begin(), sessions_.end(),
      [&session](const std::unique_ptr<AuthorizationServerSession>& as) {
        return as.get() == session;
      });
  DCHECK(it_session != sessions_.end());

  // Get the list of callbacks to run and copy the data.
  std::vector<StatusCallback> callbacks = session->TakeWaitingList();
  // We have to make a copy of `data` here because it may be an error message
  // owned by the session object deleted in the next if block.
  const std::string data2 = data;
  // Erase the session if the request failed.
  if (status != StatusCode::kOK) {
    sessions_.erase(it_session);
  }
  // Run the callbacks.
  for (auto& callback : callbacks) {
    std::move(callback).Run(status, data2);
  }
}

bool AuthorizationZoneImpl::FindAndRemovePendingAuthorization(
    const std::string& state,
    base::flat_set<std::string>& scopes,
    std::string& code_verifier) {
  std::list<PendingAuthorization>::iterator it = std::find_if(
      pending_authorizations_.begin(), pending_authorizations_.end(),
      [&state](const PendingAuthorization& pa) { return pa.state == state; });
  if (it == pending_authorizations_.end()) {
    return false;
  }
  scopes = std::move(it->scopes);
  code_verifier = std::move(it->code_verifier);
  pending_authorizations_.erase(it);
  return true;
}

void AuthorizationZoneImpl::AddContextToErrorMessage(StatusCallback& callback) {
  // Wrap the `callback` with the function PrefixForError() defined above.
  const std::string prefix = server_data_.AuthorizationServerURI().spec();
  auto new_call = base::BindOnce(&PrefixForError, std::move(callback), prefix);
  callback = std::move(new_call);
}

std::unique_ptr<AuthorizationZone> AuthorizationZone::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const GURL& authorization_server_uri,
    const std::string& client_id) {
  return std::make_unique<AuthorizationZoneImpl>(
      url_loader_factory, authorization_server_uri, client_id);
}

}  // namespace oauth2
}  // namespace printing
}  // namespace ash
