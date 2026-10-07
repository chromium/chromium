// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/profiles/profile.h"

class AccountId;
class ApplicationLocaleStorage;

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace wallpaper_handlers {

class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;
class BackdropSurpriseMeImageFetcher;
class GooglePhotosAlbumsFetcher;
class GooglePhotosSharedAlbumsFetcher;
class GooglePhotosEnabledFetcher;
class GooglePhotosPhotosFetcher;

// Delegate class for creating backdrop fetchers. Abstract class to allow
// mocking out in test.
class WallpaperFetcherDelegate {
 public:
  virtual ~WallpaperFetcherDelegate() = default;

  virtual std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const = 0;

  virtual std::unique_ptr<BackdropImageInfoFetcher>
  CreateBackdropImageInfoFetcher(const std::string& collection_id) const = 0;

  virtual std::unique_ptr<BackdropSurpriseMeImageFetcher>
  CreateBackdropSurpriseMeImageFetcher(
      const std::string& collection_id) const = 0;

  virtual std::unique_ptr<GooglePhotosAlbumsFetcher>
  CreateGooglePhotosAlbumsFetcher(Profile* profile,
                                  const AccountId& account_id) const = 0;

  virtual std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
  CreateGooglePhotosSharedAlbumsFetcher(Profile* profile,
                                        const AccountId& account_id) const = 0;

  virtual std::unique_ptr<GooglePhotosEnabledFetcher>
  CreateGooglePhotosEnabledFetcher(Profile* profile,
                                   const AccountId& account_id) const = 0;

  virtual std::unique_ptr<GooglePhotosPhotosFetcher>
  CreateGooglePhotosPhotosFetcher(Profile* profile,
                                  const AccountId& account_id) const = 0;

  virtual void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
          callback) const = 0;
};

class WallpaperFetcherDelegateImpl : public WallpaperFetcherDelegate {
 public:
  // `application_locale_storage` must not be null and must outlive `this`.
  // `shared_url_loader_factory` must not be null.
  WallpaperFetcherDelegateImpl(
      const ApplicationLocaleStorage* application_locale_storage,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory);

  WallpaperFetcherDelegateImpl(const WallpaperFetcherDelegateImpl&) = delete;
  WallpaperFetcherDelegateImpl& operator=(const WallpaperFetcherDelegateImpl&) =
      delete;

  ~WallpaperFetcherDelegateImpl() override;

  // WallpaperFetcherDelegate:
  std::unique_ptr<BackdropCollectionInfoFetcher>
  CreateBackdropCollectionInfoFetcher() const override;

  std::unique_ptr<BackdropImageInfoFetcher> CreateBackdropImageInfoFetcher(
      const std::string& collection_id) const override;

  std::unique_ptr<BackdropSurpriseMeImageFetcher>
  CreateBackdropSurpriseMeImageFetcher(
      const std::string& collection_id) const override;

  std::unique_ptr<GooglePhotosAlbumsFetcher> CreateGooglePhotosAlbumsFetcher(
      Profile* profile,
      const AccountId& account_id) const override;

  std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
  CreateGooglePhotosSharedAlbumsFetcher(
      Profile* profile,
      const AccountId& account_id) const override;

  std::unique_ptr<GooglePhotosEnabledFetcher> CreateGooglePhotosEnabledFetcher(
      Profile* profile,
      const AccountId& account_id) const override;

  std::unique_ptr<GooglePhotosPhotosFetcher> CreateGooglePhotosPhotosFetcher(
      Profile* profile,
      const AccountId& account_id) const override;

  void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
          callback) const override;

 private:
  const raw_ref<const ApplicationLocaleStorage> application_locale_storage_;
  const scoped_refptr<network::SharedURLLoaderFactory>
      shared_url_loader_factory_;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_
