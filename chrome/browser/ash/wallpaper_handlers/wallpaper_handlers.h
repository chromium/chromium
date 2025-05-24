// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"

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

  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

  // Called when the collections info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The callback upon completion of downloading and deserializing the
  // collections info.
  OnCollectionsInfoFetched callback_;

  base::WeakPtrFactory<BackdropCollectionInfoFetcher> weak_ptr_factory_{this};
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

  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

  // Called when the images info download completes.
  void OnResponseFetched(const std::string& response);

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the images info.
  const std::string collection_id_;

  // The callback upon completion of downloading and deserializing the images
  // info.
  OnImagesInfoFetched callback_;

  base::WeakPtrFactory<BackdropImageInfoFetcher> weak_ptr_factory_{this};
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

  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

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

  base::WeakPtrFactory<BackdropSurpriseMeImageFetcher> weak_ptr_factory_{this};
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
