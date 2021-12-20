// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_

#include <memory>
#include <tuple>

#include "base/callback.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class GoogleServiceAuthError;
class Profile;

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace base {
class Value;
}  // namespace base

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace network {
class SimpleURLLoader;
}  // namespace network

namespace signin {
class PrimaryAccountAccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace wallpaper_handlers {

class BackdropFetcher;

// Downloads the wallpaper collections info from the Backdrop service.
class BackdropCollectionInfoFetcher {
 public:
  using OnCollectionsInfoFetched = base::OnceCallback<
      void(bool success, const std::vector<backdrop::Collection>& collections)>;

  BackdropCollectionInfoFetcher();

  BackdropCollectionInfoFetcher(const BackdropCollectionInfoFetcher&) = delete;
  BackdropCollectionInfoFetcher& operator=(
      const BackdropCollectionInfoFetcher&) = delete;

  ~BackdropCollectionInfoFetcher();

  // Starts the fetcher.
  void Start(OnCollectionsInfoFetched callback);

 private:
  // Called when the collections info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The callback upon completion of downloading and deserializing the
  // collections info.
  OnCollectionsInfoFetched callback_;
};

// Downloads the wallpaper images info from the Backdrop service.
class BackdropImageInfoFetcher {
 public:
  using OnImagesInfoFetched =
      base::OnceCallback<void(bool success,
                              const std::string& collection_id,
                              const std::vector<backdrop::Image>& images)>;

  explicit BackdropImageInfoFetcher(const std::string& collection_id);

  BackdropImageInfoFetcher(const BackdropImageInfoFetcher&) = delete;
  BackdropImageInfoFetcher& operator=(const BackdropImageInfoFetcher&) = delete;

  ~BackdropImageInfoFetcher();

  // Starts the fetcher.
  void Start(OnImagesInfoFetched callback);

 private:
  // Called when the images info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the images info.
  const std::string collection_id_;

  // The callback upon completion of downloading and deserializing the images
  // info.
  OnImagesInfoFetched callback_;
};

// Downloads the surprise me image info from the Backdrop service.
class BackdropSurpriseMeImageFetcher {
 public:
  using OnSurpriseMeImageFetched =
      base::OnceCallback<void(bool success,
                              const backdrop::Image& image,
                              const std::string& new_resume_token)>;

  BackdropSurpriseMeImageFetcher(const std::string& collection_id,
                                 const std::string& resume_token);

  BackdropSurpriseMeImageFetcher(const BackdropSurpriseMeImageFetcher&) =
      delete;
  BackdropSurpriseMeImageFetcher& operator=(
      const BackdropSurpriseMeImageFetcher&) = delete;

  ~BackdropSurpriseMeImageFetcher();

  // Starts the fetcher.
  void Start(OnSurpriseMeImageFetched callback);

 private:
  // Called when the surprise me image info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the image info.
  const std::string collection_id_;

  // An opaque token returned by a previous image info fetch request. It is used
  // to prevent duplicate images from being returned.
  const std::string resume_token_;

  // The callback upon completion of downloading and deserializing the surprise
  // me image info.
  OnSurpriseMeImageFetched callback_;
};

// Base class for common logic among fetchers that query the Google Photos API.
template <typename... Args>
class GooglePhotosFetcher : public signin::IdentityManager::Observer {
 public:
  GooglePhotosFetcher(
      Profile* profile,
      const char* service_url,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  GooglePhotosFetcher(const GooglePhotosFetcher&) = delete;
  GooglePhotosFetcher& operator=(const GooglePhotosFetcher&) = delete;

  ~GooglePhotosFetcher() override;

  // Issues an API request if and only if one is not in progress.
  using ClientCallback = base::OnceCallback<void(Args...)>;
  virtual void AddCallbackAndStartIfNecessary(ClientCallback callback);

 protected:
  // Called when the API request finishes. `response` will be absent if there
  // was an error in sending the request, receiving the response, or parsing the
  // response; otherwise, it will hold a response in the API's specified
  // structure.
  virtual std::tuple<Args...> ParseResponse(
      absl::optional<base::Value> response) = 0;

 private:
  void OnTokenReceived(GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnJsonReceived(std::unique_ptr<std::string> response_body);
  void OnResponseReady(absl::optional<base::Value> response);

  // Profile associated with the Google Photos account that will be queried.
  Profile* const profile_;

  // Supplies `token_fetcher_` with `profile_`'s GAIA account information.
  signin::IdentityManager* const identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // API endpoint for the request this fetcher makes. Expected to outlive this
  // class and therefore not need cleanup.
  const char* const service_url_;

  // States metadata about the network request that this fetcher sends.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Called when the download finishes, successfully or in error.
  std::vector<ClientCallback> pending_client_callbacks_;

  // Used for fetching OAuth2 access tokens. Only non-null when a token
  // is being fetched.
  std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  // Used to download the client's desired information from the Google Photos
  // service. Only non-null when a download is in progress.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<GooglePhotosFetcher> weak_factory_{this};
};

// Downloads the number of photos in a user's Google Photos library.
class GooglePhotosCountFetcher : public GooglePhotosFetcher<int> {
 public:
  explicit GooglePhotosCountFetcher(Profile* profile);

  GooglePhotosCountFetcher(const GooglePhotosCountFetcher&) = delete;
  GooglePhotosCountFetcher& operator=(const GooglePhotosCountFetcher&) = delete;

  ~GooglePhotosCountFetcher() override;

 private:
  // GooglePhotosFetcher:
  std::tuple<int> ParseResponse(absl::optional<base::Value> response) override;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
