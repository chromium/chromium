// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_OAUTH2_IPP_ENDPOINT_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_PRINTING_OAUTH2_IPP_ENDPOINT_TOKEN_FETCHER_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ash/printing/oauth2/http_exchange.h"
#include "chrome/browser/ash/printing/oauth2/status_code.h"
#include "chromeos/printing/uri.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace printing {
namespace oauth2 {

// This class represents an OAuth2 session for a single IPP Endpoint. Objects
// of this class are responsible for:
// * storing basic data: endpoint's URI and scope;
// * storing current value of endpoint access token (empty string if missing);
// * storing a list of callbacks waiting for the endpoint access token;
// * sending Token Exchange Request and interpreting the response.
class IppEndpointTokenFetcher {
 public:
  // Constructor.
  IppEndpointTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& token_endpoint_uri,
      const chromeos::Uri& ipp_endpoint,
      base::flat_set<std::string>&& scope);
  // Not copyable.
  IppEndpointTokenFetcher(const IppEndpointTokenFetcher&) = delete;
  IppEndpointTokenFetcher& operator=(const IppEndpointTokenFetcher&) = delete;
  // Destructor.
  ~IppEndpointTokenFetcher();

  const chromeos::Uri& ipp_endpoint_uri() const { return ipp_endpoint_uri_; }
  const base::flat_set<std::string>& scope() const { return scope_; }

  // Returns the current endpoint access token or an empty string if the token
  // is not known yet.
  const std::string& endpoint_access_token() const {
    return endpoint_access_token_;
  }

  // Adds `callback` to the end of the waiting list. These callbacks are not
  // called internally. Instead, they can be retrieved with TakeWaitingList().
  void AddToWaitingList(StatusCallback callback);
  // Returns the waiting list by moving it. After calling this method
  // the waiting list is empty.
  std::vector<StatusCallback> TakeWaitingList();

  // Prepares and sends Token Exchange Request using `access_token`. Results are
  // returned by `callback`. If the request is successful, the callback returns
  // StatusCode::kOK and the endpoint access token as the second parameter.
  // If the server rejected the `access_token` (e.g. is expired or invalid)
  // the status StatusCode::kInvalidAccessToken is returned and the rejected
  // access token is returned in the second parameter. Otherwise, the error code
  // with a message is returned.
  void SendTokenExchangeRequest(const std::string& access_token,
                                StatusCallback callback);

 private:
  // Analyzes response for Token Exchange Request.
  void OnTokenExchangeResponse(const std::string& access_token,
                               StatusCallback callback,
                               StatusCode status);

  const GURL token_endpoint_uri_;
  const chromeos::Uri ipp_endpoint_uri_;
  const base::flat_set<std::string> scope_;

  std::string endpoint_access_token_;

  // The waiting list - a vector of waiting callbacks.
  std::vector<StatusCallback> callbacks_;

  // The object used for communication with the Authorization Server.
  HttpExchange http_exchange_;
};

}  // namespace oauth2
}  // namespace printing
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_OAUTH2_IPP_ENDPOINT_TOKEN_FETCHER_H_
