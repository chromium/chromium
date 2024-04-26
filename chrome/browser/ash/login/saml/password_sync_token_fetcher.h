// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_FETCHER_H_
#define CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class Profile;

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

namespace ash {

// Records start of polling event in UMA histogram.
void RecordStartOfSyncTokenPollingUMA(bool in_session);

// A simple fetcher object that interacts with the sync token API in order to
// create a new token, get one or verify validity of its local copy.
// The instance is not reusable, so for each StartToken(), the instance must be
// re-created. Deleting the instance cancels inflight operation.
class PasswordSyncTokenFetcher final {
 public:
  enum class RequestType { kNone, kCreateToken, kGetToken, kVerifyToken };

  // Error types will be tracked by UMA histograms.
  // TODO(crbug.com/40143230)
  enum class ErrorType {
    kMissingAccessToken,
    kRequestBodyNotSerialized,
    kServerError,
    kInvalidJson,
    kNotJsonDict,
    kCreateNoToken,
    kGetNoList,
    kGetNoToken
  };

  class Consumer {
   public:
    Consumer();
    virtual ~Consumer();

    virtual void OnTokenCreated(const std::string& sync_token) = 0;
    virtual void OnTokenFetched(const std::string& sync_token) = 0;
    virtual void OnTokenVerified(bool is_valid) = 0;
    virtual void OnApiCallFailed(ErrorType error_type) = 0;
  };

  PasswordSyncTokenFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Profile* profile,
      Consumer* consumer);
  ~PasswordSyncTokenFetcher();

  // StartTokenCreate() and StartTokenGet() require fetching OAuth access_token
  // to authorize the operation, StartTokenVerify is authenticated with API key
  // and proceeds with an empty access_token.
  void StartTokenCreate();
  void StartTokenGet();
  void StartTokenVerify(const std::string& sync_token);

 private:
  void StartAccessTokenFetch();
  void OnAccessTokenFetchComplete(GoogleServiceAuthError error,
                                  signin::AccessTokenInfo token_info);
  void FetchSyncToken(const std::string& access_token);
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void ProcessValidTokenResponse(base::Value::Dict json_response);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  const raw_ptr<Profile> profile_;
  // `consumer_` to call back when this request completes.
  const raw_ptr<Consumer> consumer_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>
      access_token_fetcher_;
  RequestType request_type_;
  // Sync token for verification request.
  std::string sync_token_;

  base::WeakPtrFactory<PasswordSyncTokenFetcher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SAML_PASSWORD_SYNC_TOKEN_FETCHER_H_
