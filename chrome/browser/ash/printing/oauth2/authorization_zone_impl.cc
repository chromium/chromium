// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/authorization_zone_impl.h"

#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
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

// Calls `callback`. Adds a prefix with `context` to an error sent in `data`.
void PrefixForError(StatusCallback callback,
                    const std::string& context,
                    StatusCode status,
                    const std::string& data) {
  std::string msg = data;
  if (status != StatusCode::kOK) {
    msg = "[" + context + "] " + data;
  }
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
    : server_data_(url_loader_factory, authorization_server_uri, client_id) {}

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
      // InitAuthorization(...)). AuthorizationProcedure() will be called inside
      // OnInitializeCallback(...).
      server_data_.Initialize(
          base::BindOnce(&AuthorizationZoneImpl::OnInitializeCallback,
                         base::Unretained(this)));
    }
  }
}

void AuthorizationZoneImpl::FinishAuthorization(const GURL& redirect_url,
                                                StatusCallback callback) {
  AddContextToErrorMessage(callback);
  // TODO(pawliczek)
  // This method is supposed to parse `redirect_url`, then match
  // PendingAuthorization structure using the `state` field and finalize
  // the authorization on the server side by obtaining the first access token.
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

void AuthorizationZoneImpl::AddContextToErrorMessage(StatusCallback& callback) {
  // Wrap the `callback` with the function PrefixForError(...) defined above.
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
