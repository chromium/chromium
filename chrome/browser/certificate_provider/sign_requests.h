// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_
#define CHROME_BROWSER_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "components/account_id/account_id.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"

namespace chromeos {
namespace certificate_provider {

class SignRequests {
 public:
  using ExtensionNameRequestIdPair = std::pair<std::string, int>;

  SignRequests();
  ~SignRequests();

  // Returns the id of the new request. The returned request id is specific to
  // the given extension.
  int AddRequest(const std::string& extension_id,
                 const scoped_refptr<net::X509Certificate>& certificate,
                 const std::optional<AccountId>& authenticating_user_account_id,
                 net::SSLPrivateKey::SignCallback callback);

  // Returns the list of requests that correspond to the authentication of the
  // given user.
  std::vector<ExtensionNameRequestIdPair> FindRequestsForAuthenticatingUser(
      const AccountId& authenticating_user_account_id) const;

  // Returns false if no request with the given id for |extension_id|
  // could be found. Otherwise removes the request and sets |certificate| and
  // |callback| to the values that were provided with AddRequest().
  bool RemoveRequest(const std::string& extension_id,
                     int request_id,
                     scoped_refptr<net::X509Certificate>* certificate,
                     net::SSLPrivateKey::SignCallback* callback);

  // Remove all pending requests for this extension and return their
  // callbacks.
  std::vector<net::SSLPrivateKey::SignCallback> RemoveAllRequests(
      const std::string& extension_id);

 private:
  struct Request {
    Request(const scoped_refptr<net::X509Certificate>& certificate,
            const std::optional<AccountId>& authenticating_user_account_id,
            net::SSLPrivateKey::SignCallback callback);
    Request(Request&& other);
    Request& operator=(Request&&);
    ~Request();

    scoped_refptr<net::X509Certificate> certificate;
    std::optional<AccountId> authenticating_user_account_id;
    net::SSLPrivateKey::SignCallback callback;
  };

  // Holds state of all sign requests to a single extension.
  struct RequestsState {
    RequestsState();
    RequestsState(RequestsState&& other);
    RequestsState& operator=(RequestsState&&);
    ~RequestsState();

    // Maps from request id to the request state.
    std::map<int, Request> pending_requests;

    // The request id that will be used for the next sign request to this
    // extension.
    int next_free_id = 0;
  };

  // Contains the state of all sign requests per extension.
  std::map<std::string, RequestsState> extension_to_requests_;
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_
