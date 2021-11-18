// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_

#include <memory>

#include "base/callback.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

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

// Downloads the number of photos a user has stored in Google Photos.
class GooglePhotosCountFetcher {
 public:
  using OnGooglePhotosCountFetched = base::OnceCallback<void(int64_t count)>;

  GooglePhotosCountFetcher();

  GooglePhotosCountFetcher(const GooglePhotosCountFetcher&) = delete;
  GooglePhotosCountFetcher& operator=(const GooglePhotosCountFetcher&) = delete;

  ~GooglePhotosCountFetcher();

  // Starts the fetcher.
  void Start(OnGooglePhotosCountFetched callback);

 private:
  // Called when the photo count download completes.
  void OnResponseFetched(const std::string& response);

  // Runs upon photo count download completion.
  OnGooglePhotosCountFetched callback_;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
