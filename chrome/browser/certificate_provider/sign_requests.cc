// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/certificate_provider/sign_requests.h"

namespace chromeos {
namespace certificate_provider {

SignRequests::Request::Request(
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<AccountId>& authenticating_user_account_id,
    net::SSLPrivateKey::SignCallback callback)
    : certificate(certificate),
      authenticating_user_account_id(authenticating_user_account_id),
      callback(std::move(callback)) {}

SignRequests::Request::Request(Request&& other) = default;

SignRequests::Request::~Request() = default;

SignRequests::Request& SignRequests::Request::operator=(Request&&) = default;

SignRequests::RequestsState::RequestsState() {}

SignRequests::RequestsState::RequestsState(RequestsState&& other) = default;

SignRequests::RequestsState::~RequestsState() {}

SignRequests::SignRequests() {}

SignRequests::~SignRequests() {}

int SignRequests::AddRequest(
    const std::string& extension_id,
    const scoped_refptr<net::X509Certificate>& certificate,
    const std::optional<AccountId>& authenticating_user_account_id,
    net::SSLPrivateKey::SignCallback callback) {
  RequestsState& state = extension_to_requests_[extension_id];
  const int request_id = state.next_free_id++;
  state.pending_requests.emplace(
      request_id, Request(certificate, authenticating_user_account_id,
                          std::move(callback)));
  return request_id;
}

std::vector<SignRequests::ExtensionNameRequestIdPair>
SignRequests::FindRequestsForAuthenticatingUser(
    const AccountId& authenticating_user_account_id) const {
  std::vector<ExtensionNameRequestIdPair> found_requests;
  for (const auto& extension_entry : extension_to_requests_) {
    const std::string& extension_id = extension_entry.first;
    const RequestsState& extension_requests = extension_entry.second;
    for (const auto& entry : extension_requests.pending_requests) {
      const int request_id = entry.first;
      const Request& request = entry.second;
      if (request.authenticating_user_account_id ==
          authenticating_user_account_id) {
        found_requests.emplace_back(extension_id, request_id);
      }
    }
  }
  return found_requests;
}

bool SignRequests::RemoveRequest(
    const std::string& extension_id,
    int request_id,
    scoped_refptr<net::X509Certificate>* certificate,
    net::SSLPrivateKey::SignCallback* callback) {
  RequestsState& state = extension_to_requests_[extension_id];
  std::map<int, Request>& pending = state.pending_requests;
  const auto it = pending.find(request_id);
  if (it == pending.end())
    return false;
  Request& request = it->second;

  *certificate = request.certificate;
  *callback = std::move(request.callback);
  pending.erase(it);
  return true;
}

std::vector<net::SSLPrivateKey::SignCallback> SignRequests::RemoveAllRequests(
    const std::string& extension_id) {
  std::vector<net::SSLPrivateKey::SignCallback> callbacks;
  for (auto& entry : extension_to_requests_[extension_id].pending_requests) {
    callbacks.push_back(std::move(entry.second.callback));
  }
  extension_to_requests_.erase(extension_id);
  return callbacks;
}

}  // namespace certificate_provider
}  // namespace chromeos
