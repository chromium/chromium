// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_DRIVE_DRIVE_SERVICE_H_
#define CHROME_BROWSER_SEARCH_DRIVE_DRIVE_SERVICE_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

// Handles requests for user Google Drive data.
class DriveService : public KeyedService {
 public:
  DriveService(const DriveService&) = delete;
  DriveService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~DriveService() override;

  using SuggestionsCallback = base::OnceCallback<void(const std::string&)>;
  // Retrieves Google Drive document suggestions from ItemSuggest API.
  void GetDriveSuggestions(SuggestionsCallback callback);
  void OnSuggestionsReceived(SuggestionsCallback callback,
                             const std::unique_ptr<std::string> json_response);

 private:
  // TODO(crbug/1164012): Use token to create request
  // with callback.
  void OnTokenReceived(SuggestionsCallback callback,
                       GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);

  // Used for fetching OAuth2 access tokens. Only non-null when a token
  // is made available, or a token is being fetched.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* identity_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_DRIVE_DRIVE_SERVICE_H_
