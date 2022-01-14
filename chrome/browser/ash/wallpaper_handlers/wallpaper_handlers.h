// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "base/callback_forward.h"
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
// Parametrized by the client callback's argument type.
template <typename T>
class GooglePhotosFetcher : public signin::IdentityManager::Observer {
 public:
  GooglePhotosFetcher(
      Profile* profile,
      const net::NetworkTrafficAnnotationTag& traffic_annotation);

  GooglePhotosFetcher(const GooglePhotosFetcher&) = delete;
  GooglePhotosFetcher& operator=(const GooglePhotosFetcher&) = delete;

  ~GooglePhotosFetcher() override;

 protected:
  // Issues an API request to `service_url` if and only if one is not in
  // progress. Each subclass is expected to write a public overload of this
  // function that prepares `service_url`--with appended query params from the
  // client if applicable--and delegates the rest of the work to this function.
  using ClientCallback = base::OnceCallback<void(T)>;
  void AddRequestAndStartIfNecessary(const std::string& service_url,
                                     ClientCallback callback);

  // Called when the API request finishes. `response` will be absent if there
  // was an error in sending the request, receiving the response, or parsing the
  // response; otherwise, it will hold a response in the API's specified
  // structure.
  virtual T ParseResponse(absl::optional<base::Value> response) = 0;

 private:
  void OnTokenReceived(const std::string& service_url,
                       GoogleServiceAuthError error,
                       signin::AccessTokenInfo token_info);
  void OnJsonReceived(const std::string& service_url,
                      std::unique_ptr<std::string> response_body);
  void OnResponseReady(const std::string& service_url,
                       absl::optional<base::Value> response);

  // Profile associated with the Google Photos account that will be queried.
  Profile* const profile_;

  // Supplies `token_fetcher_` with `profile_`'s GAIA account information.
  signin::IdentityManager* const identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // States metadata about the network request that this fetcher sends.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Callbacks for each distinct query this fetcher has been asked to make. A
  // URL's callbacks are called and then removed when the download finishes,
  // successfully or in error.
  std::map<std::string, std::vector<ClientCallback>> pending_client_callbacks_;

  // OAuth2 access token fetcher for each distinct query this fetcher has been
  // asked to make. A URL's fetcher exists until its callbacks have been called.
  std::map<std::string,
           std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher>>
      token_fetchers_;

  // Used to download the client's desired information from the Google Photos
  // service. A URL's loader exists until its callbacks have been called.
  std::map<std::string, std::unique_ptr<network::SimpleURLLoader>> url_loaders_;

  base::WeakPtrFactory<GooglePhotosFetcher> weak_factory_{this};
};

using GooglePhotosAlbumsCbkArgs =
    ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr;
// Downloads the Google Photos albums a user has created.
class GooglePhotosAlbumsFetcher
    : public GooglePhotosFetcher<GooglePhotosAlbumsCbkArgs> {
 public:
  explicit GooglePhotosAlbumsFetcher(Profile* profile);

  GooglePhotosAlbumsFetcher(const GooglePhotosAlbumsFetcher&) = delete;
  GooglePhotosAlbumsFetcher& operator=(const GooglePhotosAlbumsFetcher&) =
      delete;

  ~GooglePhotosAlbumsFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      const absl::optional<std::string>& resume_token,
      base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback);

 private:
  // GooglePhotosFetcher:
  GooglePhotosAlbumsCbkArgs ParseResponse(
      absl::optional<base::Value> response) override;
};

// Downloads the number of photos in a user's Google Photos library.
class GooglePhotosCountFetcher : public GooglePhotosFetcher<int> {
 public:
  explicit GooglePhotosCountFetcher(Profile* profile);

  GooglePhotosCountFetcher(const GooglePhotosCountFetcher&) = delete;
  GooglePhotosCountFetcher& operator=(const GooglePhotosCountFetcher&) = delete;

  ~GooglePhotosCountFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      base::OnceCallback<void(int)> callback);

 private:
  // GooglePhotosFetcher:
  int ParseResponse(absl::optional<base::Value> response) override;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
