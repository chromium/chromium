// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_
#define CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_

#include <string>
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// class Profile;
namespace network {
class SharedURLLoaderFactory;
}

class AidaClient {
 public:
  AidaClient(Profile* profile,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~AidaClient();

  void DoConversation(std::string request,
                      base::OnceCallback<void(const std::string&)> callback);

  void OverrideAidaEndpointAndScopeForTesting(const std::string& aida_endpoint,
                                              const std::string& aida_scope);

 private:
  void SendAidaRequest(std::string request,
                       base::OnceCallback<void(const std::string&)> callback);
  void AccessTokenFetchFinished(
      std::string request,
      base::OnceCallback<void(const std::string&)> callback,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo access_token_info);
  void OnSimpleLoaderComplete(
      std::string request,
      base::OnceCallback<void(const std::string&)> callback,
      std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
      std::unique_ptr<std::string> response_body);

  const raw_ref<Profile> profile_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  std::string aida_endpoint_;
  std::string aida_scope_;
  std::string access_token_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_AIDA_CLIENT_H_
