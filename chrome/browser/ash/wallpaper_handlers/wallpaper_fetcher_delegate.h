// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_
#define CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/public/cpp/wallpaper/wallpaper_controller_client.h"
#include "chrome/browser/profiles/profile.h"

class AccountId;

namespace wallpaper_handlers {

class BackdropCollectionInfoFetcher;
class BackdropImageInfoFetcher;
class BackdropSurpriseMeImageFetcher;
class GooglePhotosAlbumsFetcher;
class GooglePhotosSharedAlbumsFetcher;
class GooglePhotosEnabledFetcher;
class GooglePhotosPhotosFetcher;
class SeaPenFetcher;

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
  CreateGooglePhotosAlbumsFetcher(Profile* profile) const = 0;

  virtual std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
  CreateGooglePhotosSharedAlbumsFetcher(Profile* profile) const = 0;

  virtual std::unique_ptr<GooglePhotosEnabledFetcher>
  CreateGooglePhotosEnabledFetcher(Profile* profile) const = 0;

  virtual std::unique_ptr<GooglePhotosPhotosFetcher>
  CreateGooglePhotosPhotosFetcher(Profile* profile) const = 0;

  virtual void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
          callback) const = 0;

  virtual std::unique_ptr<SeaPenFetcher> CreateSeaPenFetcher(
      Profile* profile) const = 0;
};

class WallpaperFetcherDelegateImpl : public WallpaperFetcherDelegate {
 public:
  WallpaperFetcherDelegateImpl();

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
      Profile* profile) const override;

  std::unique_ptr<GooglePhotosSharedAlbumsFetcher>
  CreateGooglePhotosSharedAlbumsFetcher(Profile* profile) const override;

  std::unique_ptr<GooglePhotosEnabledFetcher> CreateGooglePhotosEnabledFetcher(
      Profile* profile) const override;

  std::unique_ptr<GooglePhotosPhotosFetcher> CreateGooglePhotosPhotosFetcher(
      Profile* profile) const override;

  void FetchGooglePhotosAccessToken(
      const AccountId& account_id,
      ash::WallpaperControllerClient::FetchGooglePhotosAccessTokenCallback
          callback) const override;

  std::unique_ptr<SeaPenFetcher> CreateSeaPenFetcher(
      Profile* profile) const override;
};

}  // namespace wallpaper_handlers

#endif  // CHROME_BROWSER_ASH_WALLPAPER_HANDLERS_WALLPAPER_FETCHER_DELEGATE_H_
