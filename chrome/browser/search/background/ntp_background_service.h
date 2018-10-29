// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/base/url_util.h"
#include "services/identity/public/cpp/access_token_info.h"
#include "url/gurl.h"

class GoogleServiceAuthError;

namespace identity {
class IdentityManager;
class PrimaryAccountAccessTokenFetcher;
}  // namespace identity

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// A service that connects to backends that provide background image
// information, including collection names, image urls and descriptions.
class NtpBackgroundService : public KeyedService {
 public:
  NtpBackgroundService(
      identity::IdentityManager* const identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const base::Optional<GURL>& collections_api_url_override,
      const base::Optional<GURL>& collection_images_api_url_override,
      const base::Optional<GURL>& albums_api_url_override,
      const base::Optional<GURL>& photos_api_base_url_override,
      const base::Optional<std::string>& image_options_override);
  ~NtpBackgroundService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Requests an asynchronous fetch from the network. After the update
  // completes, OnCollectionInfoAvailable will be called on the observers.
  void FetchCollectionInfo();

  // Requests an asynchronous fetch of metadata about images in the specified
  // collection. After the update completes, OnCollectionImagesAvailable will be
  // called on the observers. Requests that are made while an asynchronous fetch
  // is in progress will be dropped until the currently active loader completes.
  void FetchCollectionImageInfo(const std::string& collection_id);

  // Initially requests an access token for the signed-in user, and then
  // requests an asynchronous fetch of album information using the access token,
  // if it is available. After the fetch completes, OnAlbumInfoAvailable will be
  // called on the observers.
  void FetchAlbumInfo();

  // Initially requests an access token for the signed-in user, and then
  // requests an asynchronous fetch of photos using the access token, if it is
  // available. After the fetch completes, OnAlbumPhotosAvailable will be called
  // on the observers.
  void FetchAlbumPhotos(const std::string& album_id,
                        const std::string& photo_container_id);

  // Add/remove observers. All observers must unregister themselves before the
  // NtpBackgroundService is destroyed.
  void AddObserver(NtpBackgroundServiceObserver* observer);
  void RemoveObserver(NtpBackgroundServiceObserver* observer);

  // Check that url is contained in collection_images.
  bool IsValidBackdropUrl(const GURL& url) const;

  void AddValidBackdropUrlForTesting(const GURL& url);

  // Returns the currently cached CollectionInfo, if any.
  const std::vector<CollectionInfo>& collection_info() const {
    return collection_info_;
  }

  // Returns the currently cached CollectionImages, if any.
  const std::vector<CollectionImage>& collection_images() const {
    return collection_images_;
  }

  // Returns the error info associated with the collections request.
  const ErrorInfo& collection_error_info() const {
    return collection_error_info_;
  }

  // Returns the error info associated with the collection images request.
  const ErrorInfo& collection_images_error_info() const {
    return collection_images_error_info_;
  }

  // Returns the error info associated with the albums request.
  const ErrorInfo& album_error_info() const { return album_error_info_; }

  // Returns the error info associated with the album photos request.
  const ErrorInfo& album_photos_error_info() const {
    return album_photos_error_info_;
  }

  // Returns the currently cached AlbumInfo, if any.
  const std::vector<AlbumInfo>& album_info() const { return album_info_; }

  // Returns the currently cached AlbumPhotos, if any.
  const std::vector<AlbumPhoto>& album_photos() const { return album_photos_; }

  GURL GetCollectionsLoadURLForTesting() const;
  GURL GetImagesURLForTesting() const;
  GURL GetAlbumsURLForTesting() const;
  GURL GetAlbumPhotosApiUrlForTesting(
      const std::string& album_id,
      const std::string& photo_container_id) const;

 private:
  GURL collections_api_url_;
  GURL collection_images_api_url_;
  GURL albums_api_url_;
  GURL photos_api_base_url_;
  std::string image_options_;

  // Used to download the proto from the Backdrop service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> collections_loader_;
  std::unique_ptr<network::SimpleURLLoader> collections_image_info_loader_;
  std::unique_ptr<network::SimpleURLLoader> albums_loader_;
  std::unique_ptr<network::SimpleURLLoader> albums_photo_info_loader_;

  identity::IdentityManager* const identity_manager_;
  // The current OAuth2 token fetcher.
  std::unique_ptr<identity::PrimaryAccountAccessTokenFetcher> token_fetcher_;

  base::ObserverList<NtpBackgroundServiceObserver, true>::Unchecked observers_;

  // Callback that processes the response from the FetchCollectionInfo request,
  // refreshing the contents of collection_info_ with server-provided data.
  void OnCollectionInfoFetchComplete(
      const std::unique_ptr<std::string> response_body);

  // Callback that processes the response from the FetchCollectionImages
  // request, refreshing the contents of collection_images_ with
  // server-provided data.
  void OnCollectionImageInfoFetchComplete(
      const std::unique_ptr<std::string> response_body);

  void GetAccessTokenForAlbumCallback(
      GoogleServiceAuthError error,
      identity::AccessTokenInfo access_token_info);

  // Callback that processes the response from the FetchAlbumInfo request,
  // refreshing the contents of album_info_ with server-provided data.
  void OnAlbumInfoFetchComplete(
      const std::unique_ptr<std::string> response_body);

  void GetAccessTokenForPhotosCallback(
      GoogleServiceAuthError error,
      identity::AccessTokenInfo access_token_info);

  // Callback that processes the response from SettingPreviewRequest, refreshing
  // the contents of album_photos_ with server-provided data.
  void OnAlbumPhotosFetchComplete(
      const std::unique_ptr<std::string> response_body);

  enum class FetchComplete {
    // Indicates that asynchronous fetch of CollectionInfo has completed.
    COLLECTION_INFO,
    // Indicates that asynchronous fetch of CollectionImages has completed.
    COLLECTION_IMAGE_INFO,
    // Indicates that asynchronous fetch of AlbumInfo has completed.
    ALBUM_INFO,
    // Indicates that asynchronous fetch of AlbumPhotos has completed.
    ALBUM_PHOTOS
  };

  void NotifyObservers(FetchComplete fetch_complete);
  GURL GetAlbumPhotosApiUrl() const;
  GURL FormatAlbumPhotosBaseApiUrl(const std::string& album_id,
                                   const std::string& photo_container_id) const;

  std::vector<CollectionInfo> collection_info_;

  std::vector<CollectionImage> collection_images_;
  std::string requested_collection_id_;

  std::vector<AlbumInfo> album_info_;

  std::vector<AlbumPhoto> album_photos_;
  std::string requested_album_id_;
  std::string requested_photo_container_id_;

  ErrorInfo album_error_info_;
  ErrorInfo album_photos_error_info_;
  ErrorInfo collection_error_info_;
  ErrorInfo collection_images_error_info_;

  DISALLOW_COPY_AND_ASSIGN(NtpBackgroundService);
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
