// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/webui/personalization_app/mojom/personalization_app.mojom-forward.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "url/gurl.h"

class GoogleServiceAuthError;
class Profile;

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

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

  BackdropCollectionInfoFetcher(const BackdropCollectionInfoFetcher&) = delete;
  BackdropCollectionInfoFetcher& operator=(
      const BackdropCollectionInfoFetcher&) = delete;

  virtual ~BackdropCollectionInfoFetcher();

  // Starts the fetcher.
  virtual void Start(OnCollectionsInfoFetched callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  BackdropCollectionInfoFetcher();

 private:
  // Allow delegate to view the constructor.
  friend class WallpaperFetcherDelegateImpl;

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

  BackdropImageInfoFetcher(const BackdropImageInfoFetcher&) = delete;
  BackdropImageInfoFetcher& operator=(const BackdropImageInfoFetcher&) = delete;

  virtual ~BackdropImageInfoFetcher();

  // Starts the fetcher.
  virtual void Start(OnImagesInfoFetched callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit BackdropImageInfoFetcher(const std::string& collection_id);

 private:
  // Allow delegate to view the constructor.
  friend class WallpaperFetcherDelegateImpl;

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

  BackdropSurpriseMeImageFetcher(const BackdropSurpriseMeImageFetcher&) =
      delete;
  BackdropSurpriseMeImageFetcher& operator=(
      const BackdropSurpriseMeImageFetcher&) = delete;

  virtual ~BackdropSurpriseMeImageFetcher();

  // Starts the fetcher.
  virtual void Start(OnSurpriseMeImageFetched callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  BackdropSurpriseMeImageFetcher(const std::string& collection_id,
                                 const std::string& resume_token);

 private:
  // Allow delegate to view the constructor.
  friend class WallpaperFetcherDelegateImpl;

  // Called when the surprise me image info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the image info.
  const std::string collection_id_;

  // An opaque token returned by a previous image info fetch request. It is used
  // to prevent duplicate images from being returned. It's intentional
  // that this field is always empty. See (https://crbug.com/843537#c13).
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
  void AddRequestAndStartIfNecessary(const GURL& service_url,
                                     ClientCallback callback);

  // Called when the API request finishes. `response` will be absent if there
  // was an error in sending the request, receiving the response, or parsing the
  // response; otherwise, it will hold a response in the API's specified
  // structure.
  virtual T ParseResponse(const base::Value::Dict* response) = 0;

  // Returns the count of results contained within the specified `result`.
  virtual std::optional<size_t> GetResultCount(const T& result) = 0;

  // Contains logic for different HTTP error codes that we receive, as they can
  // carry information on the state of the user's Google Photos library.
  virtual std::optional<base::Value> CreateErrorResponse(int error_code);

  // Returns the result of the managed policy
  // WallpaperGooglePhotosIntegrationEnabled, or true if this pref is
  // not managed.
  virtual bool IsGooglePhotosIntegrationPolicyEnabled() const;

 private:
  void OnTokenReceived(
      std::unique_ptr<signin::PrimaryAccountAccessTokenFetcher> fetcher,
      const GURL& service_url,
      base::TimeTicks start_time,
      GoogleServiceAuthError error,
      signin::AccessTokenInfo token_info);
  void OnJsonReceived(std::unique_ptr<network::SimpleURLLoader> loader,
                      const GURL& service_url,
                      base::TimeTicks start_time,
                      std::unique_ptr<std::string> response_body);
  void OnResponseReady(const GURL& service_url,
                       base::TimeTicks start_time,
                       std::optional<base::Value> response);

  // Profile associated with the Google Photos account that will be queried.
  const raw_ptr<Profile> profile_;

  // Supplies `token_fetcher_` with `profile_`'s GAIA account information.
  const raw_ptr<signin::IdentityManager> identity_manager_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  // States metadata about the network request that this fetcher sends.
  const net::NetworkTrafficAnnotationTag traffic_annotation_;

  // Callbacks for each distinct query this fetcher has been asked to make. A
  // URL's callbacks are called and then removed when the download finishes,
  // successfully or in error.
  std::map<GURL, std::vector<ClientCallback>> pending_client_callbacks_;

  base::WeakPtrFactory<GooglePhotosFetcher> weak_factory_{this};
};

using GooglePhotosAlbumsCbkArgs =
    ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr;
// Downloads the Google Photos albums a user has created.
class GooglePhotosAlbumsFetcher
    : public GooglePhotosFetcher<GooglePhotosAlbumsCbkArgs> {
 public:
  GooglePhotosAlbumsFetcher(const GooglePhotosAlbumsFetcher&) = delete;
  GooglePhotosAlbumsFetcher& operator=(const GooglePhotosAlbumsFetcher&) =
      delete;

  ~GooglePhotosAlbumsFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      const std::optional<std::string>& resume_token,
      base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit GooglePhotosAlbumsFetcher(Profile* profile);

  // GooglePhotosFetcher:
  GooglePhotosAlbumsCbkArgs ParseResponse(
      const base::Value::Dict* response) override;
  std::optional<size_t> GetResultCount(
      const GooglePhotosAlbumsCbkArgs& result) override;

 private:
  // Allow delegate to see the constructor.
  friend class WallpaperFetcherDelegateImpl;
  friend class GooglePhotosAlbumsFetcherTest;

  int albums_api_refresh_counter_ = 0;
};

using GooglePhotosAlbumsCbkArgs =
    ash::personalization_app::mojom::FetchGooglePhotosAlbumsResponsePtr;
// Downloads the Google Photos albums a user has created.
class GooglePhotosSharedAlbumsFetcher
    : public GooglePhotosFetcher<GooglePhotosAlbumsCbkArgs> {
 public:
  GooglePhotosSharedAlbumsFetcher(const GooglePhotosSharedAlbumsFetcher&) =
      delete;
  GooglePhotosSharedAlbumsFetcher& operator=(
      const GooglePhotosSharedAlbumsFetcher&) = delete;

  ~GooglePhotosSharedAlbumsFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      const std::optional<std::string>& resume_token,
      base::OnceCallback<void(GooglePhotosAlbumsCbkArgs)> callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit GooglePhotosSharedAlbumsFetcher(Profile* profile);

  // GooglePhotosFetcher:
  GooglePhotosAlbumsCbkArgs ParseResponse(
      const base::Value::Dict* response) override;
  std::optional<size_t> GetResultCount(
      const GooglePhotosAlbumsCbkArgs& result) override;

 private:
  friend class WallpaperFetcherDelegateImpl;

  int shared_albums_api_refresh_counter_ = 0;
};

using ash::personalization_app::mojom::GooglePhotosEnablementState;
// Downloads whether the user is allowed to access Google Photos data.
class GooglePhotosEnabledFetcher
    : public GooglePhotosFetcher<GooglePhotosEnablementState> {
 public:
  GooglePhotosEnabledFetcher(const GooglePhotosEnabledFetcher&) = delete;
  GooglePhotosEnabledFetcher& operator=(const GooglePhotosEnabledFetcher&) =
      delete;

  ~GooglePhotosEnabledFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      base::OnceCallback<void(GooglePhotosEnablementState)> callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit GooglePhotosEnabledFetcher(Profile* profile);

  // GooglePhotosFetcher:
  GooglePhotosEnablementState ParseResponse(
      const base::Value::Dict* response) override;
  std::optional<size_t> GetResultCount(
      const GooglePhotosEnablementState& result) override;

 private:
  friend class WallpaperFetcherDelegateImpl;
  friend class GooglePhotosEnabledFetcherTest;
};

using GooglePhotosPhotosCbkArgs =
    ash::personalization_app::mojom::FetchGooglePhotosPhotosResponsePtr;
// Downloads visible photos from a user's Google Photos library.
class GooglePhotosPhotosFetcher
    : public GooglePhotosFetcher<GooglePhotosPhotosCbkArgs> {
 public:
  GooglePhotosPhotosFetcher(const GooglePhotosPhotosFetcher&) = delete;
  GooglePhotosPhotosFetcher& operator=(const GooglePhotosPhotosFetcher&) =
      delete;

  ~GooglePhotosPhotosFetcher() override;

  virtual void AddRequestAndStartIfNecessary(
      const std::optional<std::string>& item_id,
      const std::optional<std::string>& album_id,
      const std::optional<std::string>& resume_token,
      bool shuffle,
      base::OnceCallback<void(GooglePhotosPhotosCbkArgs)> callback);

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  explicit GooglePhotosPhotosFetcher(Profile* profile);

  // GooglePhotosFetcher:
  std::optional<base::Value> CreateErrorResponse(int error_code) override;
  GooglePhotosPhotosCbkArgs ParseResponse(
      const base::Value::Dict* response) override;
  std::optional<size_t> GetResultCount(
      const GooglePhotosPhotosCbkArgs& result) override;

 private:
  friend class WallpaperFetcherDelegateImpl;
  friend class GooglePhotosPhotosFetcherTest;

  int photos_api_refresh_counter_ = 0;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
