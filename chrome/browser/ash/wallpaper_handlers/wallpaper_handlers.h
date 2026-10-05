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
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"

class ApplicationLocaleStorage;

namespace backdrop {
class Collection;
class Image;
}  // namespace backdrop

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

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
  virtual void Start(OnCollectionsInfoFetched callback) = 0;

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  BackdropCollectionInfoFetcher();
};

class BackdropCollectionInfoFetcherImpl : public BackdropCollectionInfoFetcher {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  // `shared_url_loader_factory` must not be null.
  BackdropCollectionInfoFetcherImpl(
      base::PassKey<WallpaperFetcherDelegateImpl>,
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  BackdropCollectionInfoFetcherImpl(const BackdropCollectionInfoFetcherImpl&) =
      delete;
  BackdropCollectionInfoFetcherImpl& operator=(
      const BackdropCollectionInfoFetcherImpl&) = delete;

  ~BackdropCollectionInfoFetcherImpl() override;

  // BackdropCollectionInfoFetcher:
  void Start(OnCollectionsInfoFetched callback) override;

 private:
  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

  // Called when the collections info download completes.
  void OnResponseFetched(const std::string& response);

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The callback upon completion of downloading and deserializing the
  // collections info.
  OnCollectionsInfoFetched callback_;

  base::WeakPtrFactory<BackdropCollectionInfoFetcherImpl> weak_ptr_factory_{
      this};
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
  virtual void Start(OnImagesInfoFetched callback) = 0;

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  BackdropImageInfoFetcher();
};

class BackdropImageInfoFetcherImpl : public BackdropImageInfoFetcher {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  // `shared_url_loader_factory` must not be null.
  BackdropImageInfoFetcherImpl(
      base::PassKey<WallpaperFetcherDelegateImpl>,
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const std::string& collection_id);

  BackdropImageInfoFetcherImpl(const BackdropImageInfoFetcherImpl&) = delete;
  BackdropImageInfoFetcherImpl& operator=(const BackdropImageInfoFetcherImpl&) =
      delete;

  ~BackdropImageInfoFetcherImpl() override;

  // BackdropImageInfoFetcher:
  void Start(OnImagesInfoFetched callback) override;

 private:
  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

  // Called when the images info download completes.
  void OnResponseFetched(const std::string& response);

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the images info.
  const std::string collection_id_;

  // The callback upon completion of downloading and deserializing the images
  // info.
  OnImagesInfoFetched callback_;

  base::WeakPtrFactory<BackdropImageInfoFetcherImpl> weak_ptr_factory_{this};
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
  virtual void Start(OnSurpriseMeImageFetched callback) = 0;

 protected:
  // Protected constructor forces creation via `WallpaperFetcherDelegate` to
  // allow mocking in test code.
  BackdropSurpriseMeImageFetcher();
};

class BackdropSurpriseMeImageFetcherImpl
    : public BackdropSurpriseMeImageFetcher {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  // `shared_url_loader_factory` must not be null.
  BackdropSurpriseMeImageFetcherImpl(
      base::PassKey<WallpaperFetcherDelegateImpl>,
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
      const std::string& collection_id,
      const std::string& resume_token);

  BackdropSurpriseMeImageFetcherImpl(
      const BackdropSurpriseMeImageFetcherImpl&) = delete;
  BackdropSurpriseMeImageFetcherImpl& operator=(
      const BackdropSurpriseMeImageFetcherImpl&) = delete;

  ~BackdropSurpriseMeImageFetcherImpl() override;

  // BackdropSurpriseMeImageFetcher:
  void Start(OnSurpriseMeImageFetched callback) override;

 private:
  // Called when the customization_id has been read from StatisticsProvider.
  void OnGetCustomizationIdFilter(std::optional<std::string> customization_id);

  // Called when the surprise me image info download completes.
  void OnResponseFetched(const std::string& response);

  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;

  // Used to download the proto from the Backdrop service.
  std::unique_ptr<BackdropFetcher> backdrop_fetcher_;

  // The id of the collection, used as the token to fetch the image info.
  const std::string collection_id_;

  // An opaque token returned by a previous image info fetch request. It is used
  // to prevent duplicate images from being returned. It's intentional
  // that this field is always empty. See
  // (https://crbug.com/41389292#comment14).
  const std::string resume_token_;

  // The callback upon completion of downloading and deserializing the surprise
  // me image info.
  OnSurpriseMeImageFetched callback_;

  base::WeakPtrFactory<BackdropSurpriseMeImageFetcherImpl> weak_ptr_factory_{
      this};
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_HANDLERS_H_
