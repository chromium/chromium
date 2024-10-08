// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/search/background/ntp_background_data.h"
#include "chrome/browser/search/background/ntp_background_service_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "net/base/url_util.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

/**
 * Types of images that are shown on the New Tab Page's frontend.
 * This enum must match the numbering for NtpImageType in
 * enums.xml. These values are persisted to logs. Entries should not be
 * renumbered, removed or reused.
 */
enum class NtpImageType {
  kBackgroundImage = 0,
  kCollections = 1,
  kCollectionImages = 2,
  kMaxValue = kCollectionImages,
};

// A service that connects to backends that provide background image
// information, including collection names, image urls and descriptions.
class NtpBackgroundService : public KeyedService {
 public:
  NtpBackgroundService(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  NtpBackgroundService(const NtpBackgroundService&) = delete;
  NtpBackgroundService& operator=(const NtpBackgroundService&) = delete;

  ~NtpBackgroundService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Requests an asynchronous fetch from the network. After the update
  // completes, OnCollectionInfoAvailable will be called on the observers.
  virtual void FetchCollectionInfo();

  // Requests an asynchronous fetch of metadata about images in the specified
  // collection. After the update completes, OnCollectionImagesAvailable will be
  // called on the observers. Requests that are made while an asynchronous fetch
  // is in progress will be dropped until the currently active loader completes.
  virtual void FetchCollectionImageInfo(const std::string& collection_id);

  using FetchReplacementImageCallback =
      base::OnceCallback<void(const std::optional<GURL>&)>;
  // Requests an asynchronous fetch of a replacement preview image for a
  // collection.
  virtual void FetchReplacementCollectionPreviewImage(
      const std::string& collection_id,
      FetchReplacementImageCallback fetch_replacement_image_callback);

  // Requests an asynchronous fetch of metadata about the 'next' image in the
  // specified collection. The resume_token, where available, is an opaque value
  // saved from a previous GetImageFromCollectionResponse. After the update
  // completes, OnNextCollectionImageAvailable will be called on the observers.
  // Requests that are made while an asynchronous fetch is in progress will be
  // dropped until the currently active loader completes.
  void FetchNextCollectionImage(const std::string& collection_id,
                                const std::optional<std::string>& resume_token);

  // Requests an asynchronous fetch of an image's URL headers.
  virtual void VerifyImageURL(
      const GURL& url,
      base::OnceCallback<void(int)> image_url_headers_received_callback);

  // Add/remove observers. All observers must unregister themselves before the
  // NtpBackgroundService is destroyed.
  virtual void AddObserver(NtpBackgroundServiceObserver* observer);
  void RemoveObserver(NtpBackgroundServiceObserver* observer);

  // Check that |url| is contained in collection_images.
  bool IsValidBackdropUrl(const GURL& url) const;

  // Check that |collection_id| is one of the fetched collections.
  virtual bool IsValidBackdropCollection(
      const std::string& collection_id) const;

  void AddValidBackdropUrlForTesting(const GURL& url);
  void AddValidBackdropCollectionForTesting(const std::string& collection_id);

  void AddValidBackdropUrlWithThumbnailForTesting(const GURL& url,
                                                  const GURL& thumbnail_url);

  void SetNextCollectionImageForTesting(const CollectionImage& image);

  // Returns thumbnail url for the given image url if its valid. Otherwise,
  // returns empty url.
  const GURL& GetThumbnailUrl(const GURL& image_url);

  // Returns the currently cached CollectionInfo, if any.
  virtual const std::vector<CollectionInfo>& collection_info() const;

  // Returns the currently cached CollectionImages, if any.
  virtual const std::vector<CollectionImage>& collection_images() const;

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
  std::string default_image_options_;
  std::string thumbnail_image_options_;
  GURL collections_api_url_;
  GURL collection_images_api_url_;
  GURL next_image_api_url_;

  using URLLoaderList = std::list<std::unique_ptr<network::SimpleURLLoader>>;

  // Used to download the proto from the Backdrop service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> collections_loader_;
  URLLoaderList pending_collection_image_info_loaders_;
  std::unique_ptr<network::SimpleURLLoader> next_image_loader_;

  // Used to download the headers of an image in a collection.
  URLLoaderList pending_image_url_header_loaders_;

  base::ObserverList<NtpBackgroundServiceObserver, true>::Unchecked observers_;

  // Callback that processes the response from the FetchCollectionInfo request,
  // refreshing the contents of collection_info_ with server-provided data.
  void OnCollectionInfoFetchComplete(
      std::unique_ptr<std::string> response_body);

  // Callback that processes the response from the FetchCollectionImages
  // request, refreshing the contents of collection_images_ with
  // server-provided data.
  void OnCollectionImageInfoFetchComplete(
      ntp::background::GetImagesInCollectionResponse images_response,
      ErrorType error_type);

  // Callback that processes the response from VerifyCollectionImageURL request.
  void OnImageURLHeadersFetchComplete(
      URLLoaderList::iterator it,
      base::OnceCallback<void(int)> image_url_headers_received_callback,
      base::TimeTicks request_start,
      scoped_refptr<net::HttpResponseHeaders> headers);

  // Callback that processes the response of a FetchImageInfo request made by a
  // collection image whose preview image's URL is broken. The images in the
  // collection are fetched and then verified using VerifyImageUrl.
  void OnFetchReplacementCollectionPreviewImageComplete(
      FetchReplacementImageCallback fetch_replacement_image_callback,
      ntp::background::GetImagesInCollectionResponse images_response,
      ErrorType error_type);

  // Callback that processes the response of a VerifyImageUrl request made to
  // to replace a collection's preview image URL.
  void OnReplacementCollectionPreviewImageHeadersReceived(
      FetchReplacementImageCallback fetch_replacement_image_callback,
      ntp::background::GetImagesInCollectionResponse images_response,
      int replacement_image_index,
      const GURL& replacement_image_url,
      int headers_response_code);

  // Callback that processes the response from the FetchNextCollectionImage
  // request, refreshing the contents of next_collection_image_ and
  // next_resume_token_ with server-provided data.
  void OnNextImageInfoFetchComplete(std::unique_ptr<std::string> response_body);

  // Requests an asynchronous fetch of metadata about images in the specified
  // collection.
  void FetchCollectionImageInfoInternal(
      const std::string& collection_id,
      base::OnceCallback<void(ntp::background::GetImagesInCollectionResponse,
                              ErrorType)> collection_images_received_callback);

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
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_H_
