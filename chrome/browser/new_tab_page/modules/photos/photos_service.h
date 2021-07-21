// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_

#include <memory>
#include <string>

#include "base/sequence_checker.h"
#include "chrome/browser/new_tab_page/modules/photos/photos.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace signin {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace signin

// Handles requests for user Google Photos data.
class PhotosService : public KeyedService {
 public:
  PhotosService(const PhotosService&) = delete;
  PhotosService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      signin::IdentityManager* identity_manager);
  ~PhotosService() override;

  using GetMemoriesCallback = photos::mojom::PhotosHandler::GetMemoriesCallback;
  // Retrieves Google Photos memories from API.
  void GetMemories(GetMemoriesCallback callback);

 private:
  void OnTokenReceived(GetMemoriesCallback callback,
                       GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnJsonReceived(GetMemoriesCallback callback,
                      const std::string& token,
                      const std::unique_ptr<std::string> json_response);
  void OnJsonParsed(GetMemoriesCallback callback,
                    const std::string& token,
                    data_decoder::DataDecoder::ValueOrError result);

  // Used for fetching OAuth2 access tokens. Only non-null when a token
  // is made available, or a token is being fetched.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  signin::IdentityManager* identity_manager_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<PhotosService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_PHOTOS_PHOTOS_SERVICE_H_
