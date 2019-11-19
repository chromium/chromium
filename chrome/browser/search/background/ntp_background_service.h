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
#include "components/signin/public/identity_manager/access_token_info.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

// A service that connects to backends that provide background image
// information, including collection names, image urls and descriptions.
class NtpBackgroundService : public KeyedService {
 public:
  NtpBackgroundService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
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

  // Requests an asynchronous fetch of metadata about the 'next' image in the
  // specified collection. The resume_token, where available, is an opaque value
  // saved from a previous GetImageFromCollectionResponse. After the update
  // completes, OnNextCollectionImageAvailable will be called on the observers.
  // Requests that are made while an asynchronous fetch is in progress will be
  // dropped until the currently active loader completes.
  void FetchNextCollectionImage(
      const std::string& collection_id,
      const base::Optional<std::string>& resume_token);

  // Add/remove observers. All observers must unregister themselves before the
  // NtpBackgroundService is destroyed.
  void AddObserver(NtpBackgroundServiceObserver* observer);
  void RemoveObserver(NtpBackgroundServiceObserver* observer);

  // Check that |url| is contained in collection_images.
  bool IsValidBackdropUrl(const GURL& url) const;

  // Check that |collection_id| is one of the fetched collections.
  bool IsValidBackdropCollection(const std::string& collection_id) const;

  void AddValidBackdropUrlForTesting(const GURL& url);
  void AddValidBackdropCollectionForTesting(const std::string& collection_id);

  void AddValidBackdropUrlWithThumbnailForTesting(const GURL& url,
                                                  const GURL& thumbnail_url);

  void SetNextCollectionImageForTesting(const CollectionImage& image);

  // Returns thumbnail url for the given image url if its valid. Otherwise,
  // returns empty url.
  const GURL& GetThumbnailUrl(const GURL& image_url);

  // Returns the currently cached CollectionInfo, if any.
  const std::vector<CollectionInfo>& collection_info() const {
    return collection_info_;
  }

  // Returns the currently cached CollectionImages, if any.
  const std::vector<CollectionImage>& collection_images() const {
    return collection_images_;
  }

  // Returns the cached 'next' CollectionImage.
  const CollectionImage& next_image() const { return next_image_; }

  // Returns the cached resume_token to get the 'next' CollectionImage.
  const std::string& next_image_resume_token() const {
    return next_image_resume_token_;
  }

  // Returns the error info associated with the collections request.
  const ErrorInfo& collection_error_info() const {
    return collection_error_info_;
  }

  // Returns the error info associated with the collection images request.
  const ErrorInfo& collection_images_error_info() const {
    return collection_images_error_info_;
  }

  // Returns the error info associated with the next images request.
  const ErrorInfo& next_image_error_info() const {
    return next_image_error_info_;
  }

  std::string GetImageOptionsForTesting();
  GURL GetCollectionsLoadURLForTesting() const;
  GURL GetImagesURLForTesting() const;
  GURL GetNextImageURLForTesting() const;

 private:
  std::string image_options_;
  GURL collections_api_url_;
  GURL collection_images_api_url_;
  GURL next_image_api_url_;

  // Used to download the proto from the Backdrop service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> collections_loader_;
  std::unique_ptr<network::SimpleURLLoader> collections_image_info_loader_;
  std::unique_ptr<network::SimpleURLLoader> next_image_loader_;

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

  // Callback that processes the response from the FetchNextCollectionImage
  // request, refreshing the contents of next_collection_image_ and
  // next_resume_token_ with server-provided data.
  void OnNextImageInfoFetchComplete(
      const std::unique_ptr<std::string> response_body);

  enum class FetchComplete {
    // Indicates that asynchronous fetch of CollectionInfo has completed.
    COLLECTION_INFO,
    // Indicates that asynchronous fetch of CollectionImages has completed.
    COLLECTION_IMAGE_INFO,
    // Indicates that asynchronous fetch of the next CollectionImage has
    // completed.
    NEXT_IMAGE_INFO,
  };

  void NotifyObservers(FetchComplete fetch_complete);

  std::vector<CollectionInfo> collection_info_;

  std::vector<CollectionImage> collection_images_;
  std::string requested_collection_id_;

  CollectionImage next_image_;
  std::string next_image_resume_token_;
  std::string requested_next_image_collection_id_;
  std::string requested_next_image_resume_token_;

  ErrorInfo collection_error_info_;
  ErrorInfo collection_images_error_info_;
  ErrorInfo next_image_error_info_;

  DISALLOW_COPY_AND_ASSIGN(NtpBackgroundService);
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
