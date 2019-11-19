// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_
#define CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
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
  int AddRequest(
      const std::string& extension_id,
      const scoped_refptr<net::X509Certificate>& certificate,
      const base::Optional<AccountId>& authenticating_user_account_id,
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
            const base::Optional<AccountId>& authenticating_user_account_id,
            net::SSLPrivateKey::SignCallback callback);
    Request(Request&& other);
    ~Request();
    Request& operator=(Request&&);

    scoped_refptr<net::X509Certificate> certificate;
    base::Optional<AccountId> authenticating_user_account_id;
    net::SSLPrivateKey::SignCallback callback;

   private:
    DISALLOW_COPY_AND_ASSIGN(Request);
  };

  // Holds state of all sign requests to a single extension.
  struct RequestsState {
    RequestsState();
    RequestsState(RequestsState&& other);
    ~RequestsState();

    // Maps from request id to the request state.
    std::map<int, Request> pending_requests;

    // The request id that will be used for the next sign request to this
    // extension.
    int next_free_id = 0;
  };

  // Contains the state of all sign requests per extension.
  std::map<std::string, RequestsState> extension_to_requests_;

  DISALLOW_COPY_AND_ASSIGN(SignRequests);
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CERTIFICATE_PROVIDER_SIGN_REQUESTS_H_
